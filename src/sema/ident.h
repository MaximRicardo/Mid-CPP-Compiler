#pragma once

#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/type.h"

// returns whether or not the node creates 1 or more identifiers
bool Sema_node_creates_idents(const struct Parser_ASTNode *node);
// if the node creates the specifiied identifier, a ptr to its type is returned,
// else returns NULL
const struct Parser_Type *
Sema_node_creates_ident_const(const struct Parser_ASTNode *node,
                              const char *ident);

// finds the node an identifier in a scope was created in
const struct Parser_Type *
Sema_ident_type_const(const char *ident, const struct Parser_ASTNode *node,
                      const struct Lexer_Token *end);
struct Parser_Type *Sema_ident_type(const char *ident,
                                    struct Parser_ASTNode *node,
                                    const struct Lexer_Token *end);

const struct Parser_ASTNode *
Sema_ident_creation_const(const char *ident, const struct Parser_ASTNode *node,
                          const struct Lexer_Token *end);
struct Parser_ASTNode *Sema_ident_creation(const char *ident,
                                           struct Parser_ASTNode *node,
                                           const struct Lexer_Token *end);
