#pragma once

#include "generics/dynarray.h"

// arrays of ptrs are used so nodes don't get moved around in memory and ptrs
// stay valid as the AST is being constructed
MidGen_dynarray_struct_named(MidParser_ASTNodePVec, struct MidParser_ASTNode *);

struct MidParser_Allocators;
struct MidSema_Scope;

struct MidParser_ASTNodePVec MidParser_copy_nodepvec(
    const struct MidParser_ASTNodePVec *src, struct MidParser_ASTNode *dest_parent,
    struct MidSema_Scope *dest_scope, struct MidParser_Allocators *allocs);
