#include "allocator.h"
#include "generics/bumpalloc.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

void Parser_Allocators_deinit(struct Parser_Allocators *allocers)
{
    gen_bumpdeinit(&allocers->expr, Parser_Expr_deinit);
    gen_bumpdeinit(&allocers->ast, Parser_ASTNode_deinit);
    gen_bumpdeinit(&allocers->scope, Sema_Scope_deinit);
}
