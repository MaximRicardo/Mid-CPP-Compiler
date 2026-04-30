#pragma once

#include "generics/dynarray.h"

// arrays of ptrs are used so nodes don't get moved around in memory and ptrs
// stay valid as the AST is being constructed
gen_dynarray_struct_named(Parser_ASTNodePVec, struct Parser_ASTNode *);
