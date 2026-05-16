#pragma once

#include "diag.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "sema/scope.h"

// returns the type name the node creates, and NULL if it doesn't create one
const char *Sema_node_creates_type_name(const struct Parser_ASTNode *node);
const struct Parser_Type *
Sema_node_type_const(const struct Parser_ASTNode *node);
struct Parser_Type *Sema_node_type(struct Parser_ASTNode *node);

void Sema_typecheck_expr(struct Parser_Expr *expr, struct Sema_Scope *scope,
                         struct DiagVec *diags);
void Sema_typecheck_return(const struct Parser_ASTNode *node,
                           const struct Sema_Scope *scope,
                           struct DiagVec *diags);

bool Sema_can_convert(const struct Parser_Type *src,
                      enum Parser_ExprValueType src_valtype,
                      const struct Parser_Type *dest);
// a conversion sequence can have 1 of 3 ranks:
// 1) exact match,
// 2) promotion,
// 3) conversion,
int Sema_conversion_rank(const struct Parser_Type *src,
                         const struct Parser_Type *dest);
