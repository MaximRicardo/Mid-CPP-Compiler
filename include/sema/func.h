#pragma once

#include "parser/func_decl.h"

enum midsema_FuncConstexprSuitability {
    MIDSEMA_FUNCCONSTEXPR_SUITABLE,
    MIDSEMA_FUNCCONSTEXPR_NONLITERAL_RET,
    MIDSEMA_FUNCCONSTEXPR_NONLITERAL_PARAM,
    MIDSEMA_FUNCCONSTEXPR_VIRTUAL,
    MIDSEMA_FUNCCONSTEXPR_RET_IN_CTOR,
    MIDSEMA_FUNCCONSTEXPR_MULTIPLE_RET,
    MIDSEMA_FUNCCONSTEXPR_BAD_BODY,
};

enum midsema_FuncConstexprSuitability
midsema_func_constexpr_suitability(const struct midpar_FuncDecl *self);
