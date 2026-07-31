#pragma once

#include "parser/ast.h"

void MidLLVM_init_codegen(void);
void MidLLVM_codegen(const struct MidParser_ASTNode *root);
