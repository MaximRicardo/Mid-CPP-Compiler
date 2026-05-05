#include "token.h"

bool Lexer_is_numlit(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_NUMLIT_START &&
           type < LEXER_TOKENTYPE_NUMLIT_END;
}

bool Lexer_is_lit(enum Lexer_TokenType type)
{
    return Lexer_is_numlit(type);
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

bool Lexer_can_be_unary(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_ADD || type == LEXER_TOKENTYPE_SUB ||
           type == LEXER_TOKENTYPE_MUL || type == LEXER_TOKENTYPE_BITWISE_AND;
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

bool Lexer_is_named_typespec(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_STRUCT || type == LEXER_TOKENTYPE_CLASS ||
           type == LEXER_TOKENTYPE_UNION || type == LEXER_TOKENTYPE_ENUM;
}

bool Lexer_is_typestorqual(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_TYPESTORQUAL_START &&
           type < LEXER_TOKENTYPE_TYPESTORQUAL_END;
}

bool Lexer_is_typedataqual(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_TYPEDATAQUAL_START &&
           type < LEXER_TOKENTYPE_TYPEDATAQUAL_END;
}

bool Lexer_is_typequal(enum Lexer_TokenType type)
{
    return Lexer_is_typedataqual(type) || Lexer_is_typestorqual(type);
}

bool Lexer_is_typemod(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_TYPEMOD_START &&
           type < LEXER_TOKENTYPE_TYPEMOD_END;
}

bool Lexer_is_accessspec(enum Lexer_TokenType type)
{
    return type > LEXER_TOKENTYPE_ACCESSSPEC_START &&
           type < LEXER_TOKENTYPE_ACCESSSPEC_END;
}
