#include "scope.h"
#include "cgllvm/ident.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/ast.h"
#include <string.h>

void CGLLVM_Scope_deinit(struct CGLLVM_Scope *self)
{
    gen_dyndeinit(&self->idents, CGLLVM_Ident_deinit);
    gen_dyndeinit(&self->childs);
}

const struct CGLLVM_Ident *
CGLLVM_find_ident_const(const struct CGLLVM_Scope *scope, const char *name,
                        const struct CGLLVM_Scope **out_ident_scope)
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
        return CGLLVM_find_ident_const(scope->parent, name, out_ident_scope);
    return NULL;
}

struct CGLLVM_Ident *CGLLVM_find_ident(struct CGLLVM_Scope *scope,
                                       const char *name,
                                       struct CGLLVM_Scope **out_ident_scope)
{
    return (struct CGLLVM_Ident *)CGLLVM_find_ident_const(
        scope, name, (const struct CGLLVM_Scope **)out_ident_scope);
}

const struct CGLLVM_Scope *
CGLLVM_find_func_scope_const(const struct CGLLVM_Scope *scope)
{
    if (scope->node && scope->node->type == PARSER_ASTNODETYPE_FUNC_DECL)
        return scope;
    else if (scope->parent)
        return CGLLVM_find_func_scope_const(scope->parent);
    else
        return NULL;
}

struct CGLLVM_Scope *CGLLVM_find_func_scope(struct CGLLVM_Scope *scope)
{
    return (struct CGLLVM_Scope *)CGLLVM_find_func_scope_const(scope);
}

const struct CGLLVM_Scope *
CGLLVM_find_root_scope_const(const struct CGLLVM_Scope *scope)
{
    if (scope->parent)
        return scope->parent;
    else
        return scope;
}

struct CGLLVM_Scope *CGLLVM_find_root_scope(struct CGLLVM_Scope *scope)
{
    return (struct CGLLVM_Scope *)CGLLVM_find_root_scope_const(scope);
}
