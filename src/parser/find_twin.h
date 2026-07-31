#pragma once

#include "ints.h"
#include "lexer/token.h"

// finds a left parenthesis' corresponding right parenthesis
// gives up at and past end_idx
mid_isize MidParser_find_twin_paren(const struct MidLexer_Token *toks, mid_isize l_idx,
                               mid_isize end_idx);
mid_isize MidParser_find_twin_sqbracket(const struct MidLexer_Token *toks,
                                   mid_isize l_idx, mid_isize end_idx);
mid_isize MidParser_find_twin_curly(const struct MidLexer_Token *toks, mid_isize l_idx,
                               mid_isize end_idx);
mid_isize MidParser_find_twin_angle(const struct MidLexer_Token *toks, mid_isize l_idx,
                               mid_isize end_idx);

mid_isize MidParser_find_twin_generic(const struct MidLexer_Token *toks, mid_isize l_idx,
                                 mid_isize end_idx, enum MidLexer_TokenType l_type,
                                 enum MidLexer_TokenType r_type);
