#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"

const struct MidSema_Scope *MidParser_parse_scope_res_const(
    const struct MidLexer_Token *toks, mid_isize start, mid_isize *out_end,
    const struct MidSema_Scope *scope, struct MidDiag_DiagVec *diags);
struct MidSema_Scope *
MidParser_parse_scope_res(const struct MidLexer_Token *toks, mid_isize start,
                          mid_isize *out_end, struct MidSema_Scope *scope,
                          struct MidDiag_DiagVec *diags);
mid_isize MidParser_skip_scope_res(const struct MidLexer_Token *toks,
                                   mid_isize start);
