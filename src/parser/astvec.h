#pragma once

#include "generics/dynarray.h"

// arrays of ptrs are used so nodes don't get moved around in memory and ptrs
// stay valid as the AST is being constructed
gen_dynarray_struct_named(Parser_ASTNodePVec, struct Parser_ASTNode *);

struct Parser_Allocators;
struct Sema_Scope;

struct Parser_ASTNodePVec Parser_copy_nodepvec(
    const struct Parser_ASTNodePVec *src, struct Parser_ASTNode *dest_parent,
    struct Sema_Scope *dest_scope, struct Parser_Allocators *allocs);
