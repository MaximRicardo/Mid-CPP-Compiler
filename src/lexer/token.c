#include "lexer/token.h"
#include "apfloat.h"
#include "lexer/token_type.h"

void midlex_Token_deinit(struct midlex_Token *self)
{
    if (midlex_is_fltlit(self->type)) {
        mid_APFloat_deinit(&self->val.flt);
    } else if (midlex_is_numlit(self->type)) {
        mid_APInt_deinit(&self->val.i);
    }
}

bool midlex_is_fltlit(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_FLTLIT_START &&
           type < MIDLEX_TOKENTYPE_FLTLIT_END;
}

bool midlex_is_numlit(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_NUMLIT_START &&
           type < MIDLEX_TOKENTYPE_NUMLIT_END;
}

bool midlex_is_strlit(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_STRLIT_START &&
           type < MIDLEX_TOKENTYPE_STRLIT_END;
}

bool midlex_is_lit(enum midlex_TokenType type)
{
    return midlex_is_numlit(type) || midlex_is_strlit(type);
}

bool midlex_is_ternaryop(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_TERNARYOP_START &&
           type < MIDLEX_TOKENTYPE_TERNARYOP_END;
}

bool midlex_is_binop(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_BINOP_START &&
           type < MIDLEX_TOKENTYPE_BINOP_END;
}

bool midlex_is_unaryop(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_UNARYOP_START &&
           type < MIDLEX_TOKENTYPE_UNARYOP_END;
}

bool midlex_can_be_unary(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_ADD || type == MIDLEX_TOKENTYPE_SUB ||
           type == MIDLEX_TOKENTYPE_MUL || type == MIDLEX_TOKENTYPE_BITWISE_AND;
}

bool midlex_is_op(enum midlex_TokenType type)
{
    return midlex_is_binop(type) || midlex_is_unaryop(type);
}

bool midlex_is_typespec(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_TYPESPEC_START &&
           type < MIDLEX_TOKENTYPE_TYPESPEC_END;
}

bool midlex_is_named_typespec(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_STRUCT || type == MIDLEX_TOKENTYPE_CLASS ||
           type == MIDLEX_TOKENTYPE_UNION || type == MIDLEX_TOKENTYPE_ENUM;
}

bool midlex_is_typestorqual(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_TYPESTORQUAL_START &&
           type < MIDLEX_TOKENTYPE_TYPESTORQUAL_END;
}

bool midlex_is_typedataqual(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_TYPEDATAQUAL_START &&
           type < MIDLEX_TOKENTYPE_TYPEDATAQUAL_END;
}

bool midlex_is_typequal(enum midlex_TokenType type)
{
    return midlex_is_typedataqual(type) || midlex_is_typestorqual(type);
}

bool midlex_is_typemod(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_TYPEMOD_START &&
           type < MIDLEX_TOKENTYPE_TYPEMOD_END;
}

bool midlex_is_accessspec(enum midlex_TokenType type)
{
    return type > MIDLEX_TOKENTYPE_ACCESSSPEC_START &&
           type < MIDLEX_TOKENTYPE_ACCESSSPEC_END;
}
