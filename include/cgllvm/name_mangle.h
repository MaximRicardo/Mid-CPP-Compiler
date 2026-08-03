#pragma once

#include "parser/func_decl.h"
#include "parser/type.h"

#ifdef __cplusplus
extern "C" {
#endif

char *midllvm_mangle_type(const struct midpar_Type *type);
char *midllvm_mangle_func(const struct midpar_FuncDecl *func);

#ifdef __cplusplus
}
#endif
