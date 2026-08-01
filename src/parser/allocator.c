#include "allocator.h"
#include "generics/bumpalloc.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

void midpar_Allocators_deinit(struct midpar_Allocators *allocers)
{
    midgen_bumpdeinit(&allocers->expr, midpar_Expr_deinit);
    midgen_bumpdeinit(&allocers->ast, midpar_ASTNode_deinit);
    midgen_bumpdeinit(&allocers->scope, midsema_Scope_deinit);
}
