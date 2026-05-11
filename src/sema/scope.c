#include "scope.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/type.h"
#include <string.h>

void Sema_Scope_deinit(struct Sema_Scope *self)
{
    gen_dyndeinit(&self->childs);
    gen_dyndeinit(&self->idents, Sema_Ident_deinit);
}

bool Sema_is_rnce_scope(enum Sema_ScopeType type)
{
    return type == SEMA_SCOPETYPE_ROOT || type == SEMA_SCOPETYPE_NAMESPACE ||
           type == SEMA_SCOPETYPE_CLASS || type == SEMA_SCOPETYPE_ENUM;
}

const struct Sema_Scope *
Sema_closest_rnce_scope_const(const struct Sema_Scope *self)
{
    if (Sema_is_rnce_scope(self->type))
        return self;
    else if (self->parent)
        return Sema_closest_rnce_scope_const(self->parent);
    else
        return NULL;
}

struct Sema_Scope *Sema_closest_rnce_scope(struct Sema_Scope *self)
{
    return (struct Sema_Scope *)Sema_closest_rnce_scope_const(self);
}

const struct Sema_Ident *
Sema_find_ident_const(const struct Sema_Scope *scope, const char *name,
                      const struct Sema_Scope **out_ident_scope)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (!strcmp(ident->name, name)) {
            if (out_ident_scope)
                *out_ident_scope = scope;
            return ident;
        }
    }

    if (scope->parent)
        return Sema_find_ident_const(scope->parent, name, out_ident_scope);
    return NULL;
}

struct Sema_Ident *Sema_find_ident(struct Sema_Scope *scope, const char *name,
                                   struct Sema_Scope **out_ident_scope)
{
    return (struct Sema_Ident *)Sema_find_ident_const(
        scope, name, (const struct Sema_Scope **)out_ident_scope);
}

bool Sema_is_type_name(const struct Sema_Scope *scope, const char *name)
{
    auto ident = Sema_find_ident_const(scope, name, NULL);
    if (!ident)
        return NULL;

    if (ident->decl && Sema_node_creates_type_name(ident->decl))
        return ident->decl;
    return NULL;
}

struct Parser_Type Sema_type_name_type(struct Sema_Scope *scope,
                                       const char *name)
{
    struct Sema_Scope *ident_scope;
    auto ident = Sema_find_ident(scope, name, &ident_scope);
    if (!ident->decl)
        CRASH("identifier doesn't exist");

    switch (ident->type) {
    case SEMA_IDENTTYPE_CLASS: {
        auto type = Parser_toktype_to_type(ident->decl->class_.type ==
                                                   PARSER_CLASSTYPE_UNION
                                               ? LEXER_TOKENTYPE_UNION
                                               : LEXER_TOKENTYPE_CLASS);
        type.named.parent = ident_scope;
        type.named.ident = ident - ident_scope->idents.arr;
        return type;
    }

    case SEMA_IDENTTYPE_ENUM: {
        auto type = Parser_toktype_to_type(LEXER_TOKENTYPE_ENUM);
        type.named.parent = ident_scope;
        type.named.ident = ident - ident_scope->idents.arr;
        return type;
    }

    case SEMA_IDENTTYPE_TYPEDEF:
        return Parser_copy_type(&ident->decl->var_decl.type);

    default:
        break;
    }

    CRASH("identifier doesn't declare a type");
}

const struct Parser_Type *Sema_name_type_const(const struct Sema_Scope *scope,
                                               const char *name)
{
    auto ident = Sema_find_ident_const(scope, name, NULL);
    if (!ident)
        return NULL;

    if (!ident->decl)
        return NULL;
    else
        return Sema_node_type_const(ident->decl);
}

struct Parser_Type *Sema_name_type(struct Sema_Scope *scope, const char *name)
{
    return (struct Parser_Type *)Sema_name_type_const(scope, name);
}

bool Sema_tok_is_type(const struct Sema_Scope *scope,
                      const struct Lexer_Token *tok)
{
    if (Lexer_is_typespec(tok->type))
        return true;
    else if (tok->type == LEXER_TOKENTYPE_IDENTIFIER)
        return Sema_is_type_name(scope, tok->ident);
    else
        return false;
}

struct Parser_Type Sema_tok_type(struct Sema_Scope *scope,
                                 const struct Lexer_Token *tok)
{
    if (tok->type == LEXER_TOKENTYPE_IDENTIFIER)
        return Sema_type_name_type(scope, tok->ident);
    else
        return Parser_toktype_to_type(tok->type);
}

static bool are_params_same(const struct Parser_FuncDecl *a,
                            const struct Parser_FuncDecl *b)
{
    if (a->params.len != b->params.len || a->variadic != b->variadic)
        return false;

    for (isize_t i = 0; i < a->params.len; ++i) {
        if (!Parser_are_types_same(&a->params.arr[i]->var_decl.type,
                                   &b->params.arr[i]->var_decl.type))
            return false;
    }

    return true;
}

static bool are_func_decls_same(const struct Parser_FuncDecl *a,
                                const struct Parser_FuncDecl *b)
{
    if (a->is_op_overload != b->is_op_overload)
        return false;
    else if (a->is_op_overload && a->op_overload != b->op_overload)
        return false;
    else if (!a->is_op_overload && strcmp(a->name, b->name))
        return false;

    return are_params_same(a, b);
}

// only checks the scope itself and not its parents
static struct Sema_Ident *
scope_find_ident_shallow(const struct Sema_Scope *scope,
                         const struct Sema_Ident *search)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (strcmp(ident->name, search->name))
            continue;

        bool same_type = ident->type == search->type;
        if (!same_type)
            continue;

        if (search->type != SEMA_IDENTTYPE_FUNC ||
            are_func_decls_same(&search->decl->func_decl,
                                &ident->decl->func_decl))
            return ident;
    }

    return NULL;
}

struct Sema_Ident *Sema_add_ident(struct Sema_Scope *scope,
                                  const struct Sema_Ident *ident)
{
    auto old_ident = scope_find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    gen_dynpush(&scope->idents, *ident);
    return 0;
}

i32 Sema_add_ident_def(struct Sema_Scope *scope, const char *name,
                       struct Parser_ASTNode *def)
{
    auto ident = Sema_find_ident(scope, name, NULL);
    assert(ident);

    if (ident->def)
        return 1;

    ident->def = def;
    return 0;
}
