#include "scope.h"
#include "cgllvm/ident.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/ast.h"
#include <string.h>

void MidLLVM_Scope_deinit(struct MidLLVM_Scope *self)
{
    MidGen_dyndeinit(&self->idents, MidLLVM_Ident_deinit);
    MidGen_dyndeinit(&self->childs);
}

const struct MidLLVM_Ident *
MidLLVM_find_ident_const(const struct MidLLVM_Scope *scope, const char *name,
                        const struct MidLLVM_Scope **out_ident_scope)
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
        return MidLLVM_find_ident_const(scope->parent, name, out_ident_scope);
    return NULL;
}

struct MidLLVM_Ident *MidLLVM_find_ident(struct MidLLVM_Scope *scope,
                                       const char *name,
                                       struct MidLLVM_Scope **out_ident_scope)
{
    return (struct MidLLVM_Ident *)MidLLVM_find_ident_const(
        scope, name, (const struct MidLLVM_Scope **)out_ident_scope);
}

const struct MidLLVM_Scope *
MidLLVM_find_func_scope_const(const struct MidLLVM_Scope *scope)
{
    if (scope->node && scope->node->type == MIDPARSER_ASTNODETYPE_FUNC_DECL)
        return scope;
    else if (scope->parent)
        return MidLLVM_find_func_scope_const(scope->parent);
    else
        return NULL;
}

struct MidLLVM_Scope *MidLLVM_find_func_scope(struct MidLLVM_Scope *scope)
{
    return (struct MidLLVM_Scope *)MidLLVM_find_func_scope_const(scope);
}

const struct MidLLVM_Scope *
MidLLVM_find_root_scope_const(const struct MidLLVM_Scope *scope)
{
    if (scope->parent)
        return scope->parent;
    else
        return scope;
}

struct MidLLVM_Scope *MidLLVM_find_root_scope(struct MidLLVM_Scope *scope)
{
    return (struct MidLLVM_Scope *)MidLLVM_find_root_scope_const(scope);
}
