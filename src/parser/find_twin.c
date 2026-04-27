#include "find_twin.h"
#include "ints.h"
#include "lexer/token.h"

isize_t Parser_find_twin_paren(const struct Lexer_Token *toks, isize_t l_idx,
                               isize_t end_idx)
{
    i32 depth = 0;

    for (isize_t i = l_idx + 1; i < end_idx; ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_L_PAREN) {
            ++depth;
        } else if (toks[i].type == LEXER_TOKENTYPE_R_PAREN) {
            if (depth == 0)
                return i;
            --depth;
        }
    }

    return -1;
}
