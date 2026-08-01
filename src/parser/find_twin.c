#include "find_twin.h"
#include "ints.h"
#include "lexer/token.h"

mid_isize MidParser_find_twin_generic(const struct MidLexer_Token *toks,
                                      mid_isize l_idx, mid_isize end_idx,
                                      enum MidLexer_TokenType l_type,
                                      enum MidLexer_TokenType r_type)
{
    i32 depth = 0;

    for (mid_isize i = l_idx + 1;
         i < end_idx && toks[i].type != MIDLEXER_TOKENTYPE_END; ++i) {
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

mid_isize MidParser_find_twin_paren(const struct MidLexer_Token *toks,
                                    mid_isize l_idx, mid_isize end_idx)
{
    return MidParser_find_twin_generic(toks, l_idx, end_idx,
                                       MIDLEXER_TOKENTYPE_L_PAREN,
                                       MIDLEXER_TOKENTYPE_R_PAREN);
}

mid_isize MidParser_find_twin_sqbracket(const struct MidLexer_Token *toks,
                                        mid_isize l_idx, mid_isize end_idx)
{
    return MidParser_find_twin_generic(toks, l_idx, end_idx,
                                       MIDLEXER_TOKENTYPE_L_SQBRACKET,
                                       MIDLEXER_TOKENTYPE_R_SQBRACKET);
}

mid_isize MidParser_find_twin_curly(const struct MidLexer_Token *toks,
                                    mid_isize l_idx, mid_isize end_idx)
{
    return MidParser_find_twin_generic(toks, l_idx, end_idx,
                                       MIDLEXER_TOKENTYPE_L_CURLY,
                                       MIDLEXER_TOKENTYPE_R_CURLY);
}

mid_isize MidParser_find_twin_angle(const struct MidLexer_Token *toks,
                                    mid_isize l_idx, mid_isize end_idx)
{
    return MidParser_find_twin_generic(
        toks, l_idx, end_idx, MIDLEXER_TOKENTYPE_LT, MIDLEXER_TOKENTYPE_GT);
}
