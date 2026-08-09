#include "parser/find_twin.h"
#include "lexer/token.h"

midlex_TokenIter midpar_find_twin_generic(midlex_TokenIter left,
                                          midlex_TokenIter search_end,
                                          enum midlex_TokenType l_type,
                                          enum midlex_TokenType r_type)
{
    int32_t depth = 0;

    for (midlex_TokenIter i = left + 1;
         (!search_end || i < search_end) && i->type != MIDLEX_TOKENTYPE_END;
         ++i) {
        if (i->type == l_type) {
            ++depth;
        } else if (i->type == r_type) {
            if (depth == 0)
                return i;
            --depth;
        }
    }

    return nullptr;
}

midlex_TokenIter midpar_find_twin_paren(midlex_TokenIter l_paren,
                                        midlex_TokenIter search_end)
{
    return midpar_find_twin_generic(l_paren, search_end,
                                    MIDLEX_TOKENTYPE_L_PAREN,
                                    MIDLEX_TOKENTYPE_R_PAREN);
}

midlex_TokenIter midpar_find_twin_sqbracket(midlex_TokenIter l_bracket,
                                            midlex_TokenIter search_end)
{
    return midpar_find_twin_generic(l_bracket, search_end,
                                    MIDLEX_TOKENTYPE_L_SQBRACKET,
                                    MIDLEX_TOKENTYPE_R_SQBRACKET);
}

midlex_TokenIter midpar_find_twin_curly(midlex_TokenIter l_curly,
                                        midlex_TokenIter search_end)
{
    return midpar_find_twin_generic(l_curly, search_end,
                                    MIDLEX_TOKENTYPE_L_CURLY,
                                    MIDLEX_TOKENTYPE_R_CURLY);
}

midlex_TokenIter midpar_find_twin_angle(midlex_TokenIter l_angle,
                                        midlex_TokenIter search_end)
{
    return midpar_find_twin_generic(l_angle, search_end, MIDLEX_TOKENTYPE_LT,
                                    MIDLEX_TOKENTYPE_GT);
}
