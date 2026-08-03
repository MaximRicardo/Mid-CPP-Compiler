#pragma once

#include "ints.h"
#include "lexer/token.h"

// finds a left parenthesis' corresponding right parenthesis
// gives up at and past end_idx
mid_isize midpar_find_twin_paren(const struct midlex_Token *toks,
                                 mid_isize l_idx, mid_isize end_idx);
mid_isize midpar_find_twin_sqbracket(const struct midlex_Token *toks,
                                     mid_isize l_idx, mid_isize end_idx);
mid_isize midpar_find_twin_curly(const struct midlex_Token *toks,
                                 mid_isize l_idx, mid_isize end_idx);
mid_isize midpar_find_twin_angle(const struct midlex_Token *toks,
                                 mid_isize l_idx, mid_isize end_idx);

mid_isize midpar_find_twin_generic(const struct midlex_Token *toks,
                                   mid_isize l_idx, mid_isize end_idx,
                                   enum midlex_TokenType l_type,
                                   enum midlex_TokenType r_type);
