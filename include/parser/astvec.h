#pragma once

#include "generics/dynarray.h"

#ifdef __cplusplus
extern "C" {
#endif

// arrays of ptrs are used so nodes don't get moved around in memory and ptrs
// stay valid as the AST is being constructed
midgen_dynarray_struct_named(midpar_ASTNodePVec, struct midpar_ASTNode *);

struct midpar_Allocators;
struct midsema_Scope;

struct midpar_ASTNodePVec midpar_copy_nodepvec(
    const struct midpar_ASTNodePVec *src, struct midpar_ASTNode *dest_parent,
    struct midsema_Scope *dest_scope, struct midpar_Allocators *allocs);

#ifdef __cplusplus
}
#endif
