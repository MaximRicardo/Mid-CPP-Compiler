#pragma once

#include "literal.h"
#include "parser/expr.h"

// failed         - must be non-NULL, true on failure, false on success.
// returns an empty value on failure.
struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope,
                                            bool *failed);
// sets expr->constant to true on success, and to false on failure.
// returns an empty value on failure.
struct midlit_TaggedValue
midsema_eval_expr_mut(struct midpar_Expr *expr,
                      const struct midsema_Scope *scope);
// fold expr if it's constant, otherwise folds any constant sub-expressions if
// recursive is set to true.
// NOTE: doesn't require that expr->constant has been set, and will in fact set
//       the flag automatically.
void midsema_const_fold_expr(struct midpar_Expr *expr,
                             const struct midsema_Scope *scope, bool recursive);
