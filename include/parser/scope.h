#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"

#ifdef __cplusplus
extern "C" {
#endif

const struct midsema_Scope *midpar_parse_scope_res_const(
    const struct midlex_Token *toks, mid_isize start, mid_isize *out_end,
    const struct midsema_Scope *scope, struct mid_DiagVec *diags);
struct midsema_Scope *midpar_parse_scope_res(const struct midlex_Token *toks,
                                             mid_isize start,
                                             mid_isize *out_end,
                                             struct midsema_Scope *scope,
                                             struct mid_DiagVec *diags);
mid_isize midpar_skip_scope_res(const struct midlex_Token *toks,
                                mid_isize start);

#ifdef __cplusplus
}
#endif
