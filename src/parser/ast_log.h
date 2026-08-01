#pragma once

#include "parser/ast.h"
#include <stdio.h>

// logs the AST to a file
void midpar_log_ast(const struct midpar_ASTNode *root, FILE *out);
void midpar_log_node(const struct midpar_ASTNode *node, FILE *out, int indent);
