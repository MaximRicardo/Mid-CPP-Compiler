#include "find_twin.h"
#include "ints.h"
#include "lexer/token.h"

mid_isize midpar_find_twin_generic(const struct midlex_Token *toks,
                                   mid_isize l_idx, mid_isize end_idx,
                                   enum midlex_TokenType l_type,
                                   enum midlex_TokenType r_type)
{
    i32 depth = 0;

    for (mid_isize i = l_idx + 1;
         i < end_idx && toks[i].type != MIDLEX_TOKENTYPE_END; ++i) {
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

mid_isize midpar_find_twin_paren(const struct midlex_Token *toks,
                                 mid_isize l_idx, mid_isize end_idx)
{
    return midpar_find_twin_generic(toks, l_idx, end_idx,
                                    MIDLEX_TOKENTYPE_L_PAREN,
                                    MIDLEX_TOKENTYPE_R_PAREN);
}

mid_isize midpar_find_twin_sqbracket(const struct midlex_Token *toks,
                                     mid_isize l_idx, mid_isize end_idx)
{
    return midpar_find_twin_generic(toks, l_idx, end_idx,
                                    MIDLEX_TOKENTYPE_L_SQBRACKET,
                                    MIDLEX_TOKENTYPE_R_SQBRACKET);
}

mid_isize midpar_find_twin_curly(const struct midlex_Token *toks,
                                 mid_isize l_idx, mid_isize end_idx)
{
    return midpar_find_twin_generic(toks, l_idx, end_idx,
                                    MIDLEX_TOKENTYPE_L_CURLY,
                                    MIDLEX_TOKENTYPE_R_CURLY);
}

mid_isize midpar_find_twin_angle(const struct midlex_Token *toks,
                                 mid_isize l_idx, mid_isize end_idx)
{
    return midpar_find_twin_generic(toks, l_idx, end_idx, MIDLEX_TOKENTYPE_LT,
                                    MIDLEX_TOKENTYPE_GT);
}
