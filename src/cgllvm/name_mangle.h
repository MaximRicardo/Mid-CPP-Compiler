#pragma once

#include "parser/func_decl.h"
#include "parser/type.h"

char *CGLLVM_mangle_type(const struct Parser_Type *type);
char *CGLLVM_mangle_func(const struct Parser_FuncDecl *func);
