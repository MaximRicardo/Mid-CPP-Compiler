#include "token.h"
#include "lexer/token_type.h"

bool MidLexer_is_numlit(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_NUMMIDLIT_START &&
           type < MIDLEXER_TOKENTYPE_NUMMIDLIT_END;
}

bool MidLexer_is_strlit(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_STRMIDLIT_START &&
           type < MIDLEXER_TOKENTYPE_STRMIDLIT_END;
}

bool MidLexer_is_lit(enum MidLexer_TokenType type)
{
    return MidLexer_is_numlit(type) || MidLexer_is_strlit(type);
}

bool MidLexer_is_ternaryop(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_TERNARYOP_START &&
           type < MIDLEXER_TOKENTYPE_TERNARYOP_END;
}

bool MidLexer_is_binop(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_BINOP_START &&
           type < MIDLEXER_TOKENTYPE_BINOP_END;
}

bool MidLexer_is_unaryop(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_UNARYOP_START &&
           type < MIDLEXER_TOKENTYPE_UNARYOP_END;
}

bool MidLexer_can_be_unary(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_ADD || type == MIDLEXER_TOKENTYPE_SUB ||
           type == MIDLEXER_TOKENTYPE_MUL ||
           type == MIDLEXER_TOKENTYPE_BITWISE_AND;
}

bool MidLexer_is_op(enum MidLexer_TokenType type)
{
    return MidLexer_is_binop(type) || MidLexer_is_unaryop(type);
}

bool MidLexer_is_typespec(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_TYPESPEC_START &&
           type < MIDLEXER_TOKENTYPE_TYPESPEC_END;
}

bool MidLexer_is_named_typespec(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_STRUCT ||
           type == MIDLEXER_TOKENTYPE_CLASS ||
           type == MIDLEXER_TOKENTYPE_UNION || type == MIDLEXER_TOKENTYPE_ENUM;
}

bool MidLexer_is_typestorqual(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_TYPESTORQUAL_START &&
           type < MIDLEXER_TOKENTYPE_TYPESTORQUAL_END;
}

bool MidLexer_is_typedataqual(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_TYPEDATAQUAL_START &&
           type < MIDLEXER_TOKENTYPE_TYPEDATAQUAL_END;
}

bool MidLexer_is_typequal(enum MidLexer_TokenType type)
{
    return MidLexer_is_typedataqual(type) || MidLexer_is_typestorqual(type);
}

bool MidLexer_is_typemod(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_TYPEMOD_START &&
           type < MIDLEXER_TOKENTYPE_TYPEMOD_END;
}

bool MidLexer_is_accessspec(enum MidLexer_TokenType type)
{
    return type > MIDLEXER_TOKENTYPE_ACCESSSPEC_START &&
           type < MIDLEXER_TOKENTYPE_ACCESSSPEC_END;
}
