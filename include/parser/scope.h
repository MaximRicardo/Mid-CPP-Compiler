#pragma once

#include "diag.h"
#include "lexer/token.h"

#ifdef __cplusplus
extern "C" {
#endif

const struct midsema_Scope *
midpar_parse_scope_res_const(midlex_TokenIter start, midlex_TokenIter *out_end,
                             const struct midsema_Scope *scope,
                             struct mid_DiagVec *diags);
struct midsema_Scope *midpar_parse_scope_res(midlex_TokenIter start,
                                             midlex_TokenIter *out_end,
                                             struct midsema_Scope *scope,
                                             struct mid_DiagVec *diags);
midlex_TokenIter midpar_skip_scope_res(midlex_TokenIter start);

#ifdef __cplusplus
}
#endif
