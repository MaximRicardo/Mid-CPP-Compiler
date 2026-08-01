#pragma once

#include "parser/ast.h"
#include <stdio.h>

// logs the AST to a file
void MidParser_log_ast(const struct MidParser_ASTNode *root, FILE *out);
void MidParser_log_node(const struct MidParser_ASTNode *node, FILE *out,
                        int indent);
