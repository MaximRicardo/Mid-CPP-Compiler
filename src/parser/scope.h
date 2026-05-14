#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"

struct Sema_Scope *Parser_parse_scope_res(const struct Lexer_Token *toks,
                                          isize_t start, isize_t *out_end,
                                          struct Sema_Scope *scope,
                                          struct DiagVec *diags);
