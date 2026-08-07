#pragma once

#include "parser/func_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

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

bool midsema_func_is_method(const struct midpar_FuncDecl *self);
bool midsema_func_is_ctor(const struct midpar_FuncDecl *self);
bool midsema_func_is_default_ctor(const struct midpar_FuncDecl *self);
bool midsema_func_is_copy_ctor(const struct midpar_FuncDecl *self);
bool midsema_func_is_move_ctor(const struct midpar_FuncDecl *self);
// cnt_ctors    - do constructors also count?
bool midsema_func_takes_implicit_this(const struct midpar_FuncDecl *self,
                                      bool cnt_ctors);
struct midpar_Type
midsema_implicit_this_type(const struct midpar_FuncDecl *self);
bool midsema_func_is_main(const struct midpar_FuncDecl *self);
bool midsema_is_user_provided(const struct midpar_FuncDecl *self);

#ifdef __cplusplus
}
#endif
