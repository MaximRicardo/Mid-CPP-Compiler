#pragma once

#include "literal.h"
#include "parser/expr.h"

// sets expr->constant to true if the expr is constexpr
// not recursive
void midsema_set_expr_constant_flag(struct midpar_Expr *expr);
// requires that the expression is constant
struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope);
// fold expr if it's constant, otherwise folds any constant sub-expressions if
// recursive is set to true
void midsema_const_fold_expr(struct midpar_Expr *expr,
                             const struct midsema_Scope *scope, bool recursive);
