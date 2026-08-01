#pragma once

#include "diag.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"

// returns whether or not the node creates a new named type
bool MidSema_node_creates_type_name(const struct MidParser_ASTNode *node);

struct MidParser_Type MidSema_node_type(const struct MidParser_ASTNode *node,
                                        struct MidSema_Scope *scope);

void MidSema_typecheck_expr(struct MidParser_Expr *expr,
                            struct MidSema_Scope *scope,
                            struct MidDiag_DiagVec *diags);
void MidSema_typecheck_return(struct MidParser_Return *self,
                              const struct MidSema_Scope *scope,
                              struct MidDiag_DiagVec *diags);
void MidSema_typecheck_var_decl_inst(struct MidParser_VarDeclInst *inst,
                                     struct MidDiag_DiagVec *diags);

bool MidSema_can_convert(const struct MidParser_Type *src,
                         enum MidParser_ExprValueType src_valtype,
                         const struct MidParser_Type *dest);
// a conversion sequence can have 1 of 3 ranks:
// 1) exact match,
// 2) promotion,
// 3) conversion,
int MidSema_conversion_rank(const struct MidParser_Type *src,
                            const struct MidParser_Type *dest);
