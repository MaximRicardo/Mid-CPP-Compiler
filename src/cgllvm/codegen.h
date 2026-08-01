#pragma once

#include "parser/ast.h"

void midllvm_init_codegen(void);
void midllvm_codegen(const struct midpar_ASTNode *root);
