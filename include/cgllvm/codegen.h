#pragma once

#include "parser/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

void midllvm_init_codegen(void);
void midllvm_codegen(const struct midpar_ASTNode *root);

#ifdef __cplusplus
}
#endif
