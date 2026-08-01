#pragma once

#include "generics/dynarray.h"
#include "ident.h"
#include "ints.h"

struct midllvm_Scope;
midgen_dynarray_struct_named(midllvm_ScopePVec, struct midllvm_Scope *);

struct midllvm_Scope {
    struct midllvm_ScopePVec childs;
    struct midllvm_Scope *parent;
    const struct midpar_ASTNode *node;
    struct midllvm_IdentVec idents;
    mid_isize ident_idx; // refers to an ident in parent->idents.
                         // func scopes refer to the identifier holding the
                         // func name
};

void midllvm_Scope_deinit(struct midllvm_Scope *self);

const struct midllvm_Ident *
midllvm_find_ident_const(const struct midllvm_Scope *scope, const char *name,
                         const struct midllvm_Scope **out_ident_scope);
struct midllvm_Ident *
midllvm_find_ident(struct midllvm_Scope *scope, const char *name,
                   struct midllvm_Scope **out_ident_scope);

const struct midllvm_Scope *
midllvm_find_func_scope_const(const struct midllvm_Scope *scope);
struct midllvm_Scope *midllvm_find_func_scope(struct midllvm_Scope *scope);

const struct midllvm_Scope *
midllvm_find_root_scope_const(const struct midllvm_Scope *scope);
struct midllvm_Scope *midllvm_find_root_scope(struct midllvm_Scope *scope);
