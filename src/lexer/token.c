#include "token.h"

bool Lexer_is_numlit(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_NUMLIT_START &&
           type < LEXER_TOKENTYPE_NUMLIT_END;
}

bool Lexer_is_ternaryop(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_TERNARYOP_START &&
           type < LEXER_TOKENTYPE_TERNARYOP_END;
}

bool Lexer_is_binop(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_BINOP_START &&
           type < LEXER_TOKENTYPE_BINOP_END;
}

bool Lexer_is_unaryop(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_UNARYOP_START &&
           type < LEXER_TOKENTYPE_UNARYOP_END;
}

bool Lexer_is_op(enum Lexer_TokenType type)
{
    return Lexer_is_binop(type) || Lexer_is_unaryop(type);
}

bool Lexer_is_typespec(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_TYPESPEC_START &&
           type < LEXER_TOKENTYPE_TYPESPEC_END;
}

bool Lexer_is_typemod(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_TYPEMOD_START &&
           type < LEXER_TOKENTYPE_TYPEMOD_END;
}
