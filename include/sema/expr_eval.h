#pragma once

#include "literal.h"
#include "parser/expr.h"

// sets expr->ret.squals.is_constexpr to true if the expr is constexpr
bool midsema_expr_is_constexpr(struct midpar_Expr *expr);
// requires that the expression is constexpr
struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope);
void midsema_const_fold_expr(struct midpar_Expr *expr,
                             const struct midsema_Scope *scope);
