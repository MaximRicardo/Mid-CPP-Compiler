#pragma once

#include "parser/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

struct midpar_Type
midsema_instantiate_class_tmplt(struct midpar_ASTNode *tmplt,
                                const struct midpar_TmpltArgVec *args,
                                struct midpar_Allocators *allocs);

#ifdef __cplusplus
}
#endif
