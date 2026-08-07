#pragma once

#include "parser/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

struct midsema_Ident *midsema_tmplt_ident(const struct midpar_Tmplt *self);

mid_isize midsema_tmplt_param_idx(const struct midpar_Tmplt *tmplt,
                                  const char *name);

struct midpar_Type
midsema_instantiate_class_tmplt(struct midpar_ASTNode *tmplt,
                                const struct midpar_TmpltArgVec *args,
                                struct midpar_Allocators *allocs);

#ifdef __cplusplus
}
#endif
