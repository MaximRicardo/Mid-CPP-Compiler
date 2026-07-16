#include "scope.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/func_decl.h"
#include "parser/template.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/type.h"
#include <string.h>

void Sema_Scope_deinit(struct Sema_Scope *self)
{
    gen_dyndeinit(&self->childs);
    gen_dyndeinit(&self->idents, Sema_Ident_deinit);
}

void Sema_copy_scope(struct Sema_Scope *dest, const struct Sema_Scope *src,
                     struct Sema_Scope *dest_parent,
                     struct Parser_Allocators *allocs)
{
    *dest = *src;

    dest->parent = dest_parent;
    dest->idents = (struct Sema_IdentVec){};
    gen_dynreserve(&dest->idents, src->idents.len);
    dest->childs = (struct Sema_ScopePVec){};
    gen_dynreserve(&dest->childs, src->childs.len);

    for (isize_t i = 0; i < src->idents.len; ++i) {
        gen_dynpush(&dest->idents,
                    Sema_copy_ident(&src->idents.arr[i], dest, allocs));
    }

    for (isize_t i = 0; i < src->childs.len; ++i) {
        struct Sema_Scope *cpy_child;
        gen_bumpmalloc(&allocs->scope, &cpy_child);

        Sema_copy_scope(cpy_child, src->childs.arr[i], dest, allocs);

        gen_dynpush(&dest->childs, cpy_child);
    }
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

const struct Sema_Scope *
Sema_closest_scope_of_type_const(const struct Sema_Scope *self,
                                 enum Sema_ScopeType type)
{
    if (self->type == type)
        return self;
    else if (self->parent)
        return Sema_closest_scope_of_type_const(self->parent, type);
    else
        return NULL;
}

struct Sema_Scope *Sema_closest_scope_of_type(struct Sema_Scope *self,
                                              enum Sema_ScopeType type)
{
    return (struct Sema_Scope *)Sema_closest_scope_of_type_const(self, type);
}

static const struct Sema_Ident *
find_ident_in_arr(const char *name, const struct Sema_Ident *idents, isize_t n)
{
    for (isize_t i = 0; i < n; ++i) {
        auto ident = &idents[i];
        if (!strcmp(ident->name, name)) {
            return ident;
        }
    }

    return NULL;
}

static const struct Sema_Ident *
search_child_tmplt_scopes(const char *name, const struct Sema_Scope *scope)
{
    for (isize_t i = 0; i < scope->childs.len; ++i) {
        auto child = scope->childs.arr[i];
        if (child->type != SEMA_SCOPETYPE_TEMPLATE)
            continue;

        auto ident = Parser_tmplt_ident_const(&child->node->tmplt);
        if (!strcmp(ident->name, name))
            return ident;
    }

    return NULL;
}

const struct Sema_Ident *
Sema_find_ident_const(const struct Sema_Scope *scope, const char *name,
                      const struct Sema_Scope **out_ident_scope)
{
    const struct Sema_Ident *ident = NULL;
    if (!ident)
        ident = find_ident_in_arr(name, scope->idents.arr, scope->idents.len);
    if (!ident)
        ident = search_child_tmplt_scopes(name, scope);

    if (ident) {
        if (out_ident_scope)
            *out_ident_scope = scope;
        return ident;
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
        return false;

    if (ident->decl && Sema_node_creates_type_name(ident->decl))
        return true;
    return false;
}

bool Sema_is_namespace_name(const struct Sema_Scope *scope, const char *name)
{
    auto ident = Sema_find_ident_const(scope, name, NULL);
    if (!ident)
        return false;

    return ident->type == SEMA_IDENTTYPE_NAMESPACE;
}

bool Sema_ident_type(struct Sema_Scope *scope, const struct Sema_Ident *ident,
                     struct Parser_Type *out_type)
{
    if (ident->type == SEMA_IDENTTYPE_NAMESPACE || !ident->decl) {
        return false;
    } else {
        *out_type = Sema_node_type(ident->decl, scope, ident->name);
        return true;
    }
}

bool Sema_name_type(struct Sema_Scope *scope, const char *name,
                    struct Parser_Type *out_type)
{
    auto ident = Sema_find_ident_const(scope, name, NULL);
    if (!ident)
        return false;

    return Sema_ident_type(scope, ident, out_type);
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
        if (ident->decl->type == PARSER_ASTNODETYPE_VAR_DECL) {
            for (isize_t i = 0; i < ident->decl->var_decl.insts.len; ++i) {
                auto inst = &ident->decl->var_decl.insts.arr[i];
                if (!strcmp(inst->name, name))
                    return Parser_copy_type(&inst->type);
            }
        } else if (ident->decl->type == PARSER_ASTNODETYPE_TMPLT_PARAM) {
            assert(ident->decl->tmplt_param.kind == PARSER_TMPLTPARAM_TYPE);
            auto scope = ident->decl->parent->tmplt.scope;
            auto param = &ident->decl->tmplt_param.type;
            return Parser_create_templated_type(scope, param->ident_idx);
        }
        CRASH("name not typedefed in node");

    default:
        break;
    }

    CRASH("identifier doesn't declare a type");
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
        if (!Parser_are_types_same(
                &a->params.arr[i]->var_decl.insts.arr[0].type,
                &b->params.arr[i]->var_decl.insts.arr[0].type))
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
    else if (memcmp(&a->quals, &b->quals, sizeof(a->quals)))
        return false;

    return are_params_same(a, b);
}

static bool are_idents_equiv(const struct Sema_Ident *a,
                             const struct Sema_Ident *b)
{
    if (strcmp(a->name, b->name))
        return false;

    if (a->type == SEMA_IDENTTYPE_FUNC && b->type == SEMA_IDENTTYPE_FUNC &&
        !are_func_decls_same(&a->decl->func_decl, &b->decl->func_decl))
        return false;

    return true;
}

static struct Sema_Ident *
find_ident_in_child_tmplt_scopes(const struct Sema_Scope *scope,
                                 const struct Sema_Ident *search)
{
    for (isize_t i = 0; i < scope->childs.len; ++i) {
        auto child = scope->childs.arr[i];
        if (child->type != SEMA_SCOPETYPE_TEMPLATE)
            continue;

        auto ident = Parser_tmplt_ident(&child->node->tmplt);
        if (are_idents_equiv(ident, search))
            return ident;
    }

    return NULL;
}

// finds an equivalent identifier that could cause a name collision
// only checks the scope itself and not its parents
static struct Sema_Ident *find_ident_shallow(const struct Sema_Scope *scope,
                                             const struct Sema_Ident *search)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (are_idents_equiv(ident, search))
            return ident;
    }

    auto tmplt_ident = find_ident_in_child_tmplt_scopes(scope, search);
    if (tmplt_ident)
        return tmplt_ident;

    return NULL;
}

struct Sema_Ident *Sema_add_ident(struct Sema_Scope *scope,
                                  struct Sema_Ident *ident)
{
    auto old_ident = find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    gen_dynpush(&scope->idents, *ident);
    return NULL;
}

struct Sema_Ident *Sema_add_ident_copy(struct Sema_Scope *scope,
                                       const struct Sema_Ident *ident,
                                       struct Parser_Allocators *allocs)
{
    auto old_ident = find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    gen_dynpush(&scope->idents, Sema_copy_ident(ident, scope, allocs));
    return NULL;
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

const struct Sema_Scope *
Sema_resolve_scope_const(const char *name, const struct Sema_Scope *scope)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (!Sema_is_nce_ident(ident->type))
            continue;
        if (strcmp(ident->name, name))
            continue;

        return Sema_ident_scope(ident);
    }

    if (!scope->parent)
        return NULL;

    // keep searching through the parent scopes
    auto next = Sema_closest_rnce_scope_const(scope->parent);
    return Sema_resolve_scope_const(name, next);
}

struct Sema_Scope *Sema_resolve_scope(const char *name,
                                      struct Sema_Scope *scope)
{
    return (struct Sema_Scope *)Sema_resolve_scope_const(name, scope);
}

const char *Sema_scope_name(const struct Sema_Scope *scope)
{
    const char *name =
        scope->type == SEMA_SCOPETYPE_CLASS       ? scope->node->class_.name
        : scope->type == SEMA_SCOPETYPE_ENUM      ? scope->node->enum_.name
        : scope->type == SEMA_SCOPETYPE_NAMESPACE ? scope->node->nmspace.name
                                                  : NULL;

    return name;
}

void Sema_add_tmplt_params_to_scope(struct Sema_Scope *scope,
                                    const struct Sema_Scope *tmplt)
{
    for (isize_t i = 0; i < tmplt->idents.len; ++i) {
        // NOTE: doing a basic data copy should be safe (i hope)
        gen_dynpush(&scope->idents, tmplt->idents.arr[i]);
    }
}
