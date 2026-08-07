#include "sema/expr.h"
#include "macros.h"

bool midsema_is_glvalue(enum midpar_ExprValueType type)
{
    return type == MIDPAR_EXPRVALUE_LVALUE || type == MIDPAR_EXPRVALUE_XVALUE;
}

bool midsema_is_rvalue(enum midpar_ExprValueType type)
{
    return type == MIDPAR_EXPRVALUE_PRVALUE || type == MIDPAR_EXPRVALUE_XVALUE;
}

bool midsema_op_has_side_effects(enum midpar_ExprType type)
{
    return midsema_is_assignment(type) || type == MIDPAR_EXPRTYPE_POSTFIX_INC ||
           type == MIDPAR_EXPRTYPE_POSTFIX_DEC ||
           type == MIDPAR_EXPRTYPE_PREFIX_INC ||
           type == MIDPAR_EXPRTYPE_PREFIX_DEC;
}

bool midsema_is_strlit(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_STRING_LIT ||
           type == MIDPAR_EXPRTYPE_WSTRING_LIT ||
           type == MIDPAR_EXPRTYPE_STRING16_LIT ||
           type == MIDPAR_EXPRTYPE_STRING32_LIT;
}

bool midsema_is_fltlit(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_FLTLIT_START &&
           type < MIDPAR_EXPRTYPE_FLTLIT_END;
}

bool midsema_is_intlit(enum midpar_ExprType type)
{
    return midsema_is_numlit(type) && !midsema_is_fltlit(type);
}

bool midsema_is_numlit(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_NUMLIT_START &&
           type < MIDPAR_EXPRTYPE_NUMLIT_END;
}

bool midsema_is_ternaryop(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_TERNARYOP_START &&
           type < MIDPAR_EXPRTYPE_TERNARYOP_END;
}

bool midsema_is_binop(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_BINOP_START &&
           type < MIDPAR_EXPRTYPE_BINOP_END;
}

bool midsema_is_unaryop(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_UNARYOP_START &&
           type < MIDPAR_EXPRTYPE_UNARYOP_END;
}

bool midsema_is_scope_res(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_BIN_SCOPE_RES ||
           type == MIDPAR_EXPRTYPE_UNARY_SCOPE_RES;
}

bool midsema_is_op(enum midpar_ExprType type)
{
    return midsema_is_binop(type) || midsema_is_unaryop(type);
}

bool midsema_is_arith_op(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_MUL || type == MIDPAR_EXPRTYPE_DIV ||
           type == MIDPAR_EXPRTYPE_MOD || type == MIDPAR_EXPRTYPE_ADD ||
           type == MIDPAR_EXPRTYPE_SUB || type == MIDPAR_EXPRTYPE_LEFT_SHIFT ||
           type == MIDPAR_EXPRTYPE_RIGHT_SHIFT ||
           type == MIDPAR_EXPRTYPE_BITWISE_AND ||
           type == MIDPAR_EXPRTYPE_BITWISE_XOR ||
           type == MIDPAR_EXPRTYPE_BITWISE_OR ||
           type == MIDPAR_EXPRTYPE_BITWISE_NOT ||
           type == MIDPAR_EXPRTYPE_UNARY_PLUS ||
           type == MIDPAR_EXPRTYPE_UNARY_MINUS;
}

bool midsema_is_logical_op(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_LOGICAL_AND ||
           type == MIDPAR_EXPRTYPE_LOGICAL_OR ||
           type == MIDPAR_EXPRTYPE_LOGICAL_NOT;
}

bool midsema_is_comp_op(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_LT || type == MIDPAR_EXPRTYPE_GT ||
           type == MIDPAR_EXPRTYPE_LTEQ || type == MIDPAR_EXPRTYPE_GTEQ ||
           type == MIDPAR_EXPRTYPE_EQ || type == MIDPAR_EXPRTYPE_NEQ;
}

bool midsema_is_assignment(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_ASSIGN ||
           type == MIDPAR_EXPRTYPE_MUL_ASSIGN ||
           type == MIDPAR_EXPRTYPE_DIV_ASSIGN ||
           type == MIDPAR_EXPRTYPE_MOD_ASSIGN ||
           type == MIDPAR_EXPRTYPE_SUB_ASSIGN ||
           type == MIDPAR_EXPRTYPE_ADD_ASSIGN ||
           type == MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN ||
           type == MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN ||
           type == MIDPAR_EXPRTYPE_AND_ASSIGN ||
           type == MIDPAR_EXPRTYPE_OR_ASSIGN ||
           type == MIDPAR_EXPRTYPE_XOR_ASSIGN;
}

bool midsema_is_memb_sel(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_TO_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
}

enum midlit_ValueKind midsema_lit_expr_value_kind(enum midpar_ExprType type)
{
    switch (type) {
    case MIDPAR_EXPRTYPE_CHAR_LIT:
        return midtype_char_signed ? MIDLIT_VALUE_SIGNED_INT
                                   : MIDLIT_VALUE_UNSIGNED_INT;

    case MIDPAR_EXPRTYPE_WCHAR_LIT:
        return midtype_wchar_signed ? MIDLIT_VALUE_SIGNED_INT
                                    : MIDLIT_VALUE_UNSIGNED_INT;

    case MIDPAR_EXPRTYPE_CHAR16_LIT:
    case MIDPAR_EXPRTYPE_CHAR32_LIT:
    case MIDPAR_EXPRTYPE_UINT_LIT:
    case MIDPAR_EXPRTYPE_ULONG_LIT:
    case MIDPAR_EXPRTYPE_ULONGLONG_LIT:
    case MIDPAR_EXPRTYPE_NULLPTR_LIT:
        return MIDLIT_VALUE_UNSIGNED_INT;

    case MIDPAR_EXPRTYPE_INT_LIT:
    case MIDPAR_EXPRTYPE_LONG_LIT:
    case MIDPAR_EXPRTYPE_LONGLONG_LIT:
    case MIDPAR_EXPRTYPE_BOOL_LIT:
        return MIDLIT_VALUE_SIGNED_INT;

    case MIDPAR_EXPRTYPE_FLOAT_LIT:
    case MIDPAR_EXPRTYPE_DOUBLE_LIT:
    case MIDPAR_EXPRTYPE_LONGDOUBLE_LIT:
        return MIDLIT_VALUE_FLOAT;

    case MIDPAR_EXPRTYPE_STRING_LIT:
    case MIDPAR_EXPRTYPE_WSTRING_LIT:
    case MIDPAR_EXPRTYPE_STRING16_LIT:
    case MIDPAR_EXPRTYPE_STRING32_LIT:
        return MIDLIT_VALUE_STR;

    default:
        MID_CRASH("expr type is not a literal");
    }
}

int32_t midsema_op_precedence(enum midpar_ExprType op)
{
    // goes from 16 to 1
    int32_t flipped;

    switch (op) {
    case MIDPAR_EXPRTYPE_BIN_SCOPE_RES:
    case MIDPAR_EXPRTYPE_UNARY_SCOPE_RES:
        flipped = 1;
        break;

    case MIDPAR_EXPRTYPE_MEMB_SEL:
    case MIDPAR_EXPRTYPE_PTR_MEMB_SEL:
    case MIDPAR_EXPRTYPE_ARRAY_SUBSCR:
    case MIDPAR_EXPRTYPE_FUNC_CALL:
    case MIDPAR_EXPRTYPE_POSTFIX_INC:
    case MIDPAR_EXPRTYPE_POSTFIX_DEC:
    case MIDPAR_EXPRTYPE_TYPEID:
    case MIDPAR_EXPRTYPE_CONSTCAST:
    case MIDPAR_EXPRTYPE_DYNAMICCAST:
    case MIDPAR_EXPRTYPE_REINTERPRETCAST:
    case MIDPAR_EXPRTYPE_STATICCAST:
        flipped = 2;
        break;

    case MIDPAR_EXPRTYPE_SIZEOF:
    case MIDPAR_EXPRTYPE_PREFIX_INC:
    case MIDPAR_EXPRTYPE_PREFIX_DEC:
    case MIDPAR_EXPRTYPE_BITWISE_NOT:
    case MIDPAR_EXPRTYPE_LOGICAL_NOT:
    case MIDPAR_EXPRTYPE_UNARY_MINUS:
    case MIDPAR_EXPRTYPE_UNARY_PLUS:
    case MIDPAR_EXPRTYPE_REF:
    case MIDPAR_EXPRTYPE_DEREF:
    case MIDPAR_EXPRTYPE_NEW:
    case MIDPAR_EXPRTYPE_DELETE:
    case MIDPAR_EXPRTYPE_CAST:
        flipped = 3;
        break;

    case MIDPAR_EXPRTYPE_PTR_TO_MEMB_SEL:
    case MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        flipped = 4;
        break;

    case MIDPAR_EXPRTYPE_MUL:
    case MIDPAR_EXPRTYPE_DIV:
    case MIDPAR_EXPRTYPE_MOD:
        flipped = 5;
        break;

    case MIDPAR_EXPRTYPE_ADD:
    case MIDPAR_EXPRTYPE_SUB:
        flipped = 6;
        break;

    case MIDPAR_EXPRTYPE_LEFT_SHIFT:
    case MIDPAR_EXPRTYPE_RIGHT_SHIFT:
        flipped = 7;
        break;

    case MIDPAR_EXPRTYPE_LT:
    case MIDPAR_EXPRTYPE_GT:
    case MIDPAR_EXPRTYPE_LTEQ:
    case MIDPAR_EXPRTYPE_GTEQ:
        flipped = 8;
        break;

    case MIDPAR_EXPRTYPE_EQ:
    case MIDPAR_EXPRTYPE_NEQ:
        flipped = 9;
        break;

    case MIDPAR_EXPRTYPE_BITWISE_AND:
        flipped = 10;
        break;

    case MIDPAR_EXPRTYPE_BITWISE_XOR:
        flipped = 11;
        break;

    case MIDPAR_EXPRTYPE_BITWISE_OR:
        flipped = 12;
        break;

    case MIDPAR_EXPRTYPE_LOGICAL_AND:
        flipped = 13;
        break;

    case MIDPAR_EXPRTYPE_LOGICAL_OR:
        flipped = 14;
        break;

    case MIDPAR_EXPRTYPE_CONDITIONAL:
    case MIDPAR_EXPRTYPE_ASSIGN:
    case MIDPAR_EXPRTYPE_MUL_ASSIGN:
    case MIDPAR_EXPRTYPE_DIV_ASSIGN:
    case MIDPAR_EXPRTYPE_MOD_ASSIGN:
    case MIDPAR_EXPRTYPE_ADD_ASSIGN:
    case MIDPAR_EXPRTYPE_SUB_ASSIGN:
    case MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN:
    case MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
    case MIDPAR_EXPRTYPE_AND_ASSIGN:
    case MIDPAR_EXPRTYPE_XOR_ASSIGN:
    case MIDPAR_EXPRTYPE_OR_ASSIGN:
    case MIDPAR_EXPRTYPE_THROW:
        flipped = 15;
        break;

    case MIDPAR_EXPRTYPE_COMMA:
        flipped = 16;
        break;

    default:
        MID_CRASH("expr isn't an operator");
    }

    return 16 - flipped;
}

bool midsema_op_ltr_assoc(enum midpar_ExprType op)
{
    int32_t prec = midsema_op_precedence(op);
    return prec != 15 && prec != 13 && prec != 1;
}

bool midsema_expr_uses_args(enum midpar_ExprType type)
{
    return midsema_is_op(type);
}
