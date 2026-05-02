#pragma once

#include "diag.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"

bool Sema_is_typespec(const struct Lexer_Token *tok,
                      const struct Parser_ASTNode *parent);
struct Parser_Type Sema_typespec_type(const struct Lexer_Token *tok,
                                      const struct Parser_ASTNode *parent);

bool Sema_node_is_type(const struct Parser_ASTNode *node);
// crashes if the node doesn't hold a type
const char *Sema_node_type_name(const struct Parser_ASTNode *node);

// returns an AST node holding the type declaration or definition, or NULL if
// none were found
// stops searching at end
const struct Parser_ASTNode *
Sema_find_type_const(const char *name, const struct Parser_ASTNode *node,
                     const struct Lexer_Token *end);
struct Parser_ASTNode *Sema_find_type(const char *name,
                                      struct Parser_ASTNode *node,
                                      const struct Lexer_Token *end);

void Sema_typecheck_expr(struct Parser_Expr *expr,
                         struct Parser_ASTNode *parent, struct DiagVec *diags);
void Sema_typecheck_root(struct Parser_ASTNode *node, struct DiagVec *diags);
void Sema_typecheck_var_decl(struct Parser_VarDecl *decl,
                             struct Parser_ASTNode *node,
                             struct DiagVec *diags);
void Sema_typecheck_func_decl(struct Parser_FuncDecl *decl,
                              struct Parser_ASTNode *node,
                              struct DiagVec *diags);
void Sema_typecheck_class(struct Parser_Class *self, struct DiagVec *diags);
void Sema_typecheck_node(struct Parser_ASTNode *node, struct DiagVec *diags);
