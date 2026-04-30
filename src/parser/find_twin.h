#pragma once

#include "ints.h"
#include "lexer/token.h"

// finds a left parenthesis' corresponding right parenthesis
// gives up at and past end_idx
isize_t Parser_find_twin_paren(const struct Lexer_Token *toks, isize_t l_idx,
                               isize_t end_idx);
isize_t Parser_find_twin_sqbracket(const struct Lexer_Token *toks,
                                   isize_t l_idx, isize_t end_idx);
isize_t Parser_find_twin_curly(const struct Lexer_Token *toks, isize_t l_idx,
                               isize_t end_idx);

isize_t Parser_find_twin_generic(const struct Lexer_Token *toks, isize_t l_idx,
                                 isize_t end_idx, enum Lexer_TokenType l_type,
                                 enum Lexer_TokenType r_type);
