#pragma once

#include "generics/bumpalloc.h"

gen_bumpalloc_struct_named(Parser_ExprBump, struct Parser_Expr);
gen_bumpalloc_struct_named(Parser_ASTNodeBump, struct Parser_ASTNode);
gen_bumpalloc_struct_named(Sema_ScopeBump, struct Sema_Scope);

struct Parser_Allocators {
    struct Parser_ExprBump expr;
    struct Parser_ASTNodeBump ast;
    struct Sema_ScopeBump scope;
};

void Parser_Allocators_deinit(struct Parser_Allocators *allocers);
