#include "find_twin.h"
#include "ints.h"
#include "lexer/token.h"

isize_t Parser_find_twin_generic(const struct Lexer_Token *toks, isize_t l_idx,
                                 isize_t end_idx, enum Lexer_TokenType l_type,
                                 enum Lexer_TokenType r_type)
{
    i32 depth = 0;

    for (isize_t i = l_idx + 1;
         i < end_idx && toks[i].type != LEXER_TOKENTYPE_END; ++i) {
        if (toks[i].type == l_type) {
            ++depth;
        } else if (toks[i].type == r_type) {
            if (depth == 0)
                return i;
            --depth;
        }
    }

    return -1;
}

isize_t Parser_find_twin_paren(const struct Lexer_Token *toks, isize_t l_idx,
                               isize_t end_idx)
{
    return Parser_find_twin_generic(
        toks, l_idx, end_idx, LEXER_TOKENTYPE_L_PAREN, LEXER_TOKENTYPE_R_PAREN);
}

isize_t Parser_find_twin_sqbracket(const struct Lexer_Token *toks,
                                   isize_t l_idx, isize_t end_idx)
{
    return Parser_find_twin_generic(toks, l_idx, end_idx,
                                    LEXER_TOKENTYPE_L_SQBRACKET,
                                    LEXER_TOKENTYPE_R_SQBRACKET);
}
