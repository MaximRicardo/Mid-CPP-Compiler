#pragma once

#include "diag.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"

// returns whether or not the node creates a new named type
bool Sema_node_creates_type_name(const struct Parser_ASTNode *node);

// if the node can contain multiple types, a name is needed to select which
// declaration to use, otherwise name can be NULL.
struct Parser_Type Sema_node_type(const struct Parser_ASTNode *node,
                                  struct Sema_Scope *scope, const char *name);

void Sema_typecheck_expr(struct Parser_Expr *expr, struct Sema_Scope *scope,
                         struct DiagVec *diags);
void Sema_typecheck_return(const struct Parser_ASTNode *node,
                           const struct Sema_Scope *scope,
                           struct DiagVec *diags);
void Sema_typecheck_var_decl_inst(struct Parser_VarDeclInst *inst,
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
