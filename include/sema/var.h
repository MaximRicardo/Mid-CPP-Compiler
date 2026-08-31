#pragma once

#include "parser/var_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

bool midsema_var_inst_is_inited(const struct midpar_VarDeclInst *self);
bool midsema_var_inst_is_constexpr_inited(
    const struct midpar_VarDeclInst *self);

#ifdef __cplusplus
}
#endif
