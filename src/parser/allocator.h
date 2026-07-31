#pragma once

#include "generics/bumpalloc.h"

MidGen_bumpalloc_struct_named(MidParser_ExprBump, struct MidParser_Expr);
MidGen_bumpalloc_struct_named(MidParser_ASTNodeBump, struct MidParser_ASTNode);
MidGen_bumpalloc_struct_named(MidSema_ScopeBump, struct MidSema_Scope);

struct MidParser_Allocators {
    struct MidParser_ExprBump expr;
    struct MidParser_ASTNodeBump ast;
    struct MidSema_ScopeBump scope;
};

void MidParser_Allocators_deinit(struct MidParser_Allocators *allocers);
