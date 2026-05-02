#pragma once

#include "parser/ast.h"
#include "parser/expr.h"
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
Sema_ident_type_const(const char *ident,
                      const struct Parser_ASTNode *search_node);
struct Parser_Type *Sema_ident_type(const char *ident,
                                    struct Parser_ASTNode *search_node);

const struct Parser_ASTNode *
Sema_ident_creation_const(const char *ident,
                          const struct Parser_ASTNode *search_node);
struct Parser_ASTNode *Sema_ident_creation(const char *ident,
                                           struct Parser_ASTNode *node);

const struct Parser_ASTNode *
Sema_ident_def_const(const char *name,
                     const struct Parser_ASTNode *search_node);
struct Parser_ASTNode *Sema_ident_def(const char *name,
                                      struct Parser_ASTNode *node);

// doesn't have a const version cuz that would be annoying to implement
struct Parser_ASTNodePVec Sema_op_overloads(enum Parser_ExprType op,
                                            struct Parser_ASTNode *node);
