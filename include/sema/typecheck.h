#pragma once

#include "diag.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

// returns whether or not the node creates a new named type
bool midsema_node_creates_new_type(const struct midpar_ASTNode *node);
// returns the type associated with the node.
// for example, a var decl inst like "int *x" has the type "int *" associated
// with it. and a class node like "class Name {...}" has the type "class Name"
// associated with it.
struct midpar_Type midsema_node_type(const struct midpar_ASTNode *node,
                                     struct midsema_Scope *scope);

void midsema_typecheck_expr(struct midpar_Expr *expr,
                            struct midsema_Scope *scope,
                            struct mid_DiagVec *diags);
void midsema_typecheck_return(struct midpar_Return *self,
                              const struct midsema_Scope *scope,
                              struct mid_DiagVec *diags);
void midsema_typecheck_var_decl_inst(struct midpar_VarDeclInst *inst,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags);
void midsema_typecheck_func_decl(struct midpar_FuncDecl *func,
                                 struct mid_DiagVec *diags);
void midsema_typecheck_func_body(struct midpar_FuncDecl *func,
                                 struct mid_DiagVec *diags);

bool midsema_can_convert(const struct midpar_Type *src,
                         enum midpar_ExprValueType src_valtype,
                         const struct midpar_Type *dest);
// a conversion sequence can have 1 of 3 ranks:
// 1) exact match,
// 2) promotion,
// 3) conversion,
int midsema_conversion_rank(const struct midpar_Type *src,
                            const struct midpar_Type *dest);

#ifdef __cplusplus
}
#endif
