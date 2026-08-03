#pragma once

#include "generics/bumpalloc.h"

midgen_bumpalloc_struct_named(midpar_ExprBump, struct midpar_Expr);
midgen_bumpalloc_struct_named(midpar_ASTNodeBump, struct midpar_ASTNode);
midgen_bumpalloc_struct_named(midsema_ScopeBump, struct midsema_Scope);

struct midpar_Allocators {
    struct midpar_ExprBump expr;
    struct midpar_ASTNodeBump ast;
    struct midsema_ScopeBump scope;
};

void midpar_Allocators_deinit(struct midpar_Allocators *allocers);
