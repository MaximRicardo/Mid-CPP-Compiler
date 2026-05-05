#pragma once

#include "diag.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"

// returns the type name the node creates, and NULL if it doesn't create one
const char *Sema_node_creates_type_name(const struct Parser_ASTNode *node);
const struct Parser_Type *
Sema_node_type_const(const struct Parser_ASTNode *node);
struct Parser_Type *Sema_node_type(struct Parser_ASTNode *node);

void Sema_typecheck_expr(struct Parser_Expr *expr, struct Sema_Scope *scope,
                         struct DiagVec *diags);
void Sema_typecheck_root(struct Parser_ASTNode *node, struct Sema_Scope *scope,
                         struct DiagVec *diags);
void Sema_typecheck_var_decl(struct Parser_VarDecl *decl,
                             struct Sema_Scope *scope, struct DiagVec *diags);
void Sema_typecheck_func_decl(struct Parser_FuncDecl *decl,
                              struct DiagVec *diags);
void Sema_typecheck_class(struct Parser_Class *self, struct DiagVec *diags);
void Sema_typecheck_node(struct Parser_ASTNode *node, struct Sema_Scope *scope,
                         struct DiagVec *diags);
