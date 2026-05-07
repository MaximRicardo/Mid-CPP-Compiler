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

/*
static void scope_ptr_deinit(struct Sema_Scope **ptr)
{
    Sema_Scope_deinit(*ptr);
    // free(*ptr);
}
*/

void Sema_Scope_deinit(struct Sema_Scope *self)
{
    gen_dyndeinit(&self->childs);
    gen_dyndeinit(&self->idents);
}

const struct Sema_Ident *Sema_find_ident_const(const struct Sema_Scope *scope,
                                               const char *name)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (!strcmp(ident->name, name))
            return ident;
    }

    if (scope->parent)
        return Sema_find_ident_const(scope->parent, name);
    return NULL;
}

struct Sema_Ident *Sema_find_ident(struct Sema_Scope *scope, const char *name)
{
    return (struct Sema_Ident *)Sema_find_ident_const(scope, name);
}

bool Sema_is_type_name(const struct Sema_Scope *scope, const char *name)
{
    auto ident = Sema_find_ident_const(scope, name);
    if (!ident)
        return NULL;

    if (ident->decl && Sema_node_creates_type_name(ident->decl))
        return ident->decl;
    return NULL;
}

struct Parser_Type Sema_type_name_type(struct Sema_Scope *scope,
                                       const char *name)
{
    auto ident = Sema_find_ident(scope, name);
    if (!ident->decl)
        CRASH("identifier doesn't exist");

    switch (ident->type) {
    case SEMA_IDENTTYPE_CLASS: {
        auto type = Parser_toktype_to_type(ident->decl->class_.type ==
                                                   PARSER_CLASSTYPE_UNION
                                               ? LEXER_TOKENTYPE_UNION
                                               : LEXER_TOKENTYPE_CLASS);
        type.ident = ident;
        return type;
    }

    case SEMA_IDENTTYPE_ENUM: {
        auto type = Parser_toktype_to_type(LEXER_TOKENTYPE_ENUM);
        type.ident = ident;
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
    auto ident = Sema_find_ident_const(scope, name);
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

// only checks the scope itself and not its parents
static struct Sema_Ident *
scope_find_ident_shallow(const struct Sema_Scope *scope, const char *name)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (!strcmp(ident->name, name))
            return ident;
    }

    return NULL;
}

struct Sema_Ident *Sema_add_ident(struct Sema_Scope *scope,
                                  const struct Sema_Ident *ident)
{
    auto old_ident = scope_find_ident_shallow(scope, ident->name);
    if (old_ident)
        return old_ident;

    gen_dynpush(&scope->idents, *ident);
    return 0;
}

i32 Sema_add_ident_def(struct Sema_Scope *scope, const char *name,
                       struct Parser_ASTNode *def)
{
    auto ident = Sema_find_ident(scope, name);
    assert(ident);

    if (ident->def)
        return 1;

    ident->def = def;
    return 0;
}
