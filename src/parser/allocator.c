#include "allocator.h"
#include "generics/bumpalloc.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

void MidParser_Allocators_deinit(struct MidParser_Allocators *allocers)
{
    MidGen_bumpdeinit(&allocers->expr, MidParser_Expr_deinit);
    MidGen_bumpdeinit(&allocers->ast, MidParser_ASTNode_deinit);
    MidGen_bumpdeinit(&allocers->scope, MidSema_Scope_deinit);
}
