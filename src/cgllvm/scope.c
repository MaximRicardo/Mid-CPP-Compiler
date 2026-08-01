#include "scope.h"
#include "cgllvm/ident.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/ast.h"
#include <string.h>

void midllvm_Scope_deinit(struct midllvm_Scope *self)
{
    midgen_dyndeinit(&self->idents, midllvm_Ident_deinit);
    midgen_dyndeinit(&self->childs);
}

const struct midllvm_Ident *
midllvm_find_ident_const(const struct midllvm_Scope *scope, const char *name,
                         const struct midllvm_Scope **out_ident_scope)
{
    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (!strcmp(ident->name, name)) {
            if (out_ident_scope)
                *out_ident_scope = scope;
            return ident;
        }
    }

    if (scope->parent)
        return midllvm_find_ident_const(scope->parent, name, out_ident_scope);
    return NULL;
}

struct midllvm_Ident *midllvm_find_ident(struct midllvm_Scope *scope,
                                         const char *name,
                                         struct midllvm_Scope **out_ident_scope)
{
    return (struct midllvm_Ident *)midllvm_find_ident_const(
        scope, name, (const struct midllvm_Scope **)out_ident_scope);
}

const struct midllvm_Scope *
midllvm_find_func_scope_const(const struct midllvm_Scope *scope)
{
    if (scope->node && scope->node->type == MIDPAR_ASTNODETYPE_FUNC_DECL)
        return scope;
    else if (scope->parent)
        return midllvm_find_func_scope_const(scope->parent);
    else
        return NULL;
}

struct midllvm_Scope *midllvm_find_func_scope(struct midllvm_Scope *scope)
{
    return (struct midllvm_Scope *)midllvm_find_func_scope_const(scope);
}

const struct midllvm_Scope *
midllvm_find_root_scope_const(const struct midllvm_Scope *scope)
{
    if (scope->parent)
        return scope->parent;
    else
        return scope;
}

struct midllvm_Scope *midllvm_find_root_scope(struct midllvm_Scope *scope)
{
    return (struct midllvm_Scope *)midllvm_find_root_scope_const(scope);
}
