#pragma once

#include "generics/dynarray.h"
#include "ident.h"

gen_dynarray_struct_named(CGLLVM_ScopePVec, struct CGLLVM_Scope *);
struct CGLLVM_Scope {
    struct CGLLVM_ScopePVec childs;
    struct CGLLVM_Scope *parent;
    const struct Parser_ASTNode *node;
    struct CGLLVM_IdentVec idents;
};

void CGLLVM_Scope_deinit(struct CGLLVM_Scope *self);

const struct CGLLVM_Ident *
CGLLVM_find_ident_const(const struct CGLLVM_Scope *scope, const char *name,
                        const struct CGLLVM_Scope **out_ident_scope);
