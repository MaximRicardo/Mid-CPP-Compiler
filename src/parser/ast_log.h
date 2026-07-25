#pragma once

#include "parser/ast.h"
#include <stdio.h>

// logs the AST to a file
void Parser_log_ast(const struct Parser_ASTNode *root, FILE *out);
void Parser_log_node(const struct Parser_ASTNode *node, FILE *out, int indent);
