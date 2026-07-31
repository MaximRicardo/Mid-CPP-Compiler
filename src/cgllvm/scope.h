#pragma once

#include "generics/dynarray.h"
#include "ident.h"
#include "ints.h"

struct MidLLVM_Scope;
MidGen_dynarray_struct_named(MidLLVM_ScopePVec, struct MidLLVM_Scope *);

struct MidLLVM_Scope {
    struct MidLLVM_ScopePVec childs;
    struct MidLLVM_Scope *parent;
    const struct MidParser_ASTNode *node;
    struct MidLLVM_IdentVec idents;
    mid_isize ident_idx; // refers to an ident in parent->idents.
                         // func scopes refer to the identifier holding the
                         // func name
};

void MidLLVM_Scope_deinit(struct MidLLVM_Scope *self);

const struct MidLLVM_Ident *
MidLLVM_find_ident_const(const struct MidLLVM_Scope *scope, const char *name,
                         const struct MidLLVM_Scope **out_ident_scope);
struct MidLLVM_Ident *
MidLLVM_find_ident(struct MidLLVM_Scope *scope, const char *name,
                   struct MidLLVM_Scope **out_ident_scope);

const struct MidLLVM_Scope *
MidLLVM_find_func_scope_const(const struct MidLLVM_Scope *scope);
struct MidLLVM_Scope *MidLLVM_find_func_scope(struct MidLLVM_Scope *scope);

const struct MidLLVM_Scope *
MidLLVM_find_root_scope_const(const struct MidLLVM_Scope *scope);
struct MidLLVM_Scope *MidLLVM_find_root_scope(struct MidLLVM_Scope *scope);
