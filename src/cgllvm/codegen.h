#pragma once

#include "parser/ast.h"

void CGLLVM_init_codegen(void);
void CGLLVM_codegen(const struct Parser_ASTNode *root);
