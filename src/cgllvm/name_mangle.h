#pragma once

#include "parser/func_decl.h"
#include "parser/type.h"

char *MidLLVM_mangle_type(const struct MidParser_Type *type);
char *MidLLVM_mangle_func(const struct MidParser_FuncDecl *func);
