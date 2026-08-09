#pragma once

#include "lexer/token.h"

#ifdef __cplusplus
extern "C" {
#endif

// finds a left parenthesis's corresponding right parenthesis.
// gives up at and past search_end, unless search_end is NULL in which case
// the search ends when it finds MIDLEX_TOKENTYPE_END
midlex_TokenIter midpar_find_twin_paren(midlex_TokenIter l_paren,
                                        midlex_TokenIter search_end);
// finds a left square bracket's corresponding right square bracket.
// gives up at and past search_end, unless search_end is NULL in which case
// the search ends when it finds MIDLEX_TOKENTYPE_END
midlex_TokenIter midpar_find_twin_sqbracket(midlex_TokenIter l_bracket,
                                            midlex_TokenIter search_end);
// finds a left curly brackets's corresponding right curly bracket.
// gives up at and past search_end, unless search_end is NULL in which case
// the search ends when it finds MIDLEX_TOKENTYPE_END
midlex_TokenIter midpar_find_twin_curly(midlex_TokenIter l_curly,
                                        midlex_TokenIter search_end);
// finds a left angle bracket's corresponding right angle bracket.
// gives up at and past search_end, unless search_end is NULL in which case
// the search ends when it finds MIDLEX_TOKENTYPE_END
midlex_TokenIter midpar_find_twin_angle(midlex_TokenIter l_angle,
                                        midlex_TokenIter search_end);

// gives up at and past search_end, unless search_end is NULL in which case
// the search ends when it finds MIDLEX_TOKENTYPE_END
midlex_TokenIter midpar_find_twin_generic(midlex_TokenIter left,
                                          midlex_TokenIter search_end,
                                          enum midlex_TokenType l_type,
                                          enum midlex_TokenType r_type);

#ifdef __cplusplus
}
#endif
