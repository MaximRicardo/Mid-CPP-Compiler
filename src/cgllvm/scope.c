#include "scope.h"
#include "cgllvm/ident.h"
#include "generics/dynarray.h"
#include "ints.h"
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
