#pragma once

#include "generics/bumpalloc.h"

gen_bumpalloc_struct_named(Parser_ExprBump, struct Parser_Expr);
gen_bumpalloc_struct_named(Parser_ASTNodeBump, struct Parser_ASTNode);
gen_bumpalloc_struct_named(Sema_ScopeBump, struct Sema_Scope);

struct Parser_Bump {
    struct Parser_ExprBump expr;
    struct Parser_ASTNodeBump ast;
    struct Sema_ScopeBump scope;
};

// TODO: make this not be a global
extern struct Parser_Bump Parser_bumps;

void Parser_Bump_deinit(struct Parser_Bump *bumps);
