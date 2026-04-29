#pragma once

#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/type.h"

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
