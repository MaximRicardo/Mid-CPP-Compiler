#include "bump.h"
#include "generics/bumpalloc.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

struct Parser_Bump Parser_bumps = {};

void Parser_Bump_deinit(struct Parser_Bump *bumps)
{
    gen_bumpdeinit(&bumps->expr, Parser_Expr_deinit);
    gen_bumpdeinit(&bumps->ast, Parser_ASTNode_deinit);
    gen_bumpdeinit(&bumps->scope, Sema_Scope_deinit);
}
