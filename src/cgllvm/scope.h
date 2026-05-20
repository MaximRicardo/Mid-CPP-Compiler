#pragma once

#include "generics/dynarray.h"
#include "ident.h"
#include "ints.h"

gen_dynarray_struct_named(CGLLVM_ScopePVec, struct CGLLVM_Scope *);
struct CGLLVM_Scope {
    struct CGLLVM_ScopePVec childs;
    struct CGLLVM_Scope *parent;
    const struct Parser_ASTNode *node;
    struct CGLLVM_IdentVec idents;
    isize_t ident_idx; // refers to an ident in parent->idents.
                       // func scopes refer to the identifier holding the
                       // func name
};

void CGLLVM_Scope_deinit(struct CGLLVM_Scope *self);

const struct CGLLVM_Ident *
CGLLVM_find_ident_const(const struct CGLLVM_Scope *scope, const char *name,
                        const struct CGLLVM_Scope **out_ident_scope);
struct CGLLVM_Ident *CGLLVM_find_ident(struct CGLLVM_Scope *scope,
                                       const char *name,
                                       struct CGLLVM_Scope **out_ident_scope);

const struct CGLLVM_Scope *
CGLLVM_find_func_scope_const(const struct CGLLVM_Scope *scope);
struct CGLLVM_Scope *CGLLVM_find_func_scope(struct CGLLVM_Scope *scope);

const struct CGLLVM_Scope *
CGLLVM_find_root_scope_const(const struct CGLLVM_Scope *scope);
struct CGLLVM_Scope *CGLLVM_find_root_scope(struct CGLLVM_Scope *scope);
