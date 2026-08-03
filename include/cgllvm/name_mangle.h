#pragma once

#include "parser/func_decl.h"
#include "parser/type.h"

char *midllvm_mangle_type(const struct midpar_Type *type);
char *midllvm_mangle_func(const struct midpar_FuncDecl *func);
