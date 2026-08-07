#pragma once

#include "parser/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

bool midsema_is_glvalue(enum midpar_ExprValueType type);
bool midsema_is_rvalue(enum midpar_ExprValueType type);

bool midsema_is_strlit(enum midpar_ExprType type);
bool midsema_is_fltlit(enum midpar_ExprType type);
bool midsema_is_intlit(enum midpar_ExprType type); // any integral type
bool midsema_is_numlit(enum midpar_ExprType type);
bool midsema_is_ternaryop(enum midpar_ExprType type);
bool midsema_is_binop(enum midpar_ExprType type);
bool midsema_is_unaryop(enum midpar_ExprType type);
bool midsema_is_scope_res(enum midpar_ExprType type);
bool midsema_is_op(enum midpar_ExprType type);
bool midsema_is_arith_op(enum midpar_ExprType type);
bool midsema_is_logical_op(enum midpar_ExprType type);
bool midsema_is_comp_op(enum midpar_ExprType type);
// checks for both regular and compound assignment
bool midsema_is_assignment(enum midpar_ExprType type);
bool midsema_is_memb_sel(enum midpar_ExprType type);

bool midsema_op_has_side_effects(enum midpar_ExprType type);

enum midlit_ValueKind midsema_lit_expr_value_kind(enum midpar_ExprType type);

// goes from 0 to 15, where 15 is the highest precedence
int32_t midsema_op_precedence(enum midpar_ExprType op);
bool midsema_op_ltr_assoc(enum midpar_ExprType op);

bool midsema_expr_uses_args(enum midpar_ExprType type);

#ifdef __cplusplus
}
#endif
