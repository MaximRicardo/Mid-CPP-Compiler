#include "expr_type.h"

const char *MidParser_exprtype_name(enum MidParser_ExprType type)
{
    switch (type) {
    case MIDPARSER_EXPRTYPE_NUMMIDLIT_START:
        return "NUMMIDLIT_START";

    case MIDPARSER_EXPRTYPE_CHAR_LIT:
        return "CHAR_LIT";

    case MIDPARSER_EXPRTYPE_WCHAR_LIT:
        return "WCHAR_LIT";

    case MIDPARSER_EXPRTYPE_CHAR16_LIT:
        return "CHAR16_LIT";

    case MIDPARSER_EXPRTYPE_CHAR32_LIT:
        return "CHAR32_LIT";

    case MIDPARSER_EXPRTYPE_STRING_LIT:
        return "STRING_LIT";

    case MIDPARSER_EXPRTYPE_WSTRING_LIT:
        return "WSTRING_LIT";

    case MIDPARSER_EXPRTYPE_STRING16_LIT:
        return "STRING16_LIT";

    case MIDPARSER_EXPRTYPE_STRING32_LIT:
        return "STRING32_LIT";

    case MIDPARSER_EXPRTYPE_INT_LIT:
        return "INT_LIT";

    case MIDPARSER_EXPRTYPE_UINT_LIT:
        return "UINT_LIT";

    case MIDPARSER_EXPRTYPE_LONG_LIT:
        return "LONG_INT";

    case MIDPARSER_EXPRTYPE_ULONG_LIT:
        return "ULONG_INT";

    case MIDPARSER_EXPRTYPE_LONGLONG_LIT:
        return "LONGLONG_INT";

    case MIDPARSER_EXPRTYPE_ULONGLONG_LIT:
        return "ULONGLONG_INT";

    case MIDPARSER_EXPRTYPE_FLOAT_LIT:
        return "FLOAT_LIT";

    case MIDPARSER_EXPRTYPE_DOUBLE_LIT:
        return "DOUBLE_LIT";

    case MIDPARSER_EXPRTYPE_LONGDOUBLE_LIT:
        return "LONGDOUBLE_LIT";

    case MIDPARSER_EXPRTYPE_BOOL_LIT:
        return "BOOL_LIT";

    case MIDPARSER_EXPRTYPE_NULLPTR_LIT:
        return "NULLPTR_LIT";

    case MIDPARSER_EXPRTYPE_NUMMIDLIT_END:
        return "NUMMIDLIT_END";

    case MIDPARSER_EXPRTYPE_IDENTIFIER:
        return "IDENTIFIER";

    case MIDPARSER_EXPRTYPE_THIS:
        return "THIS";

    case MIDPARSER_EXPRTYPE_TERNARYOP_START:
        return "TERNARYOP_START";

    case MIDPARSER_EXPRTYPE_CONDITIONAL:
        return "CONDITIONAL";

    case MIDPARSER_EXPRTYPE_TERNARYOP_END:
        return "TERNARYOP_END";

    case MIDPARSER_EXPRTYPE_BINOP_START:
        return "BINOP_START";

    case MIDPARSER_EXPRTYPE_BIN_SCOPE_RES:
        return "BIN_SCOPE_RES";

    case MIDPARSER_EXPRTYPE_MEMB_SEL:
        return "MEMB_SEL";

    case MIDPARSER_EXPRTYPE_PTR_MEMB_SEL:
        return "PTR_MEMB_SEL";

    case MIDPARSER_EXPRTYPE_ARRAY_SUBSCR:
        return "ARRAY_SUBSCR";

    case MIDPARSER_EXPRTYPE_NEW_ARR:
        return "NEW_ARR";

    case MIDPARSER_EXPRTYPE_DELETE_ARR:
        return "DELETE_ARR";

    case MIDPARSER_EXPRTYPE_PTR_TO_MEMB_SEL:
        return "PTR_TO_MEMB_SEL";

    case MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        return "PTR_TO_PTR_MEMB_SEL";

    case MIDPARSER_EXPRTYPE_MUL:
        return "MUL";

    case MIDPARSER_EXPRTYPE_DIV:
        return "DIV";

    case MIDPARSER_EXPRTYPE_MOD:
        return "MOD";

    case MIDPARSER_EXPRTYPE_ADD:
        return "ADD";

    case MIDPARSER_EXPRTYPE_SUB:
        return "SUB";

    case MIDPARSER_EXPRTYPE_LEFT_SHIFT:
        return "LEFT_SHIFT";

    case MIDPARSER_EXPRTYPE_RIGHT_SHIFT:
        return "RIGHT_SHIFT";

    case MIDPARSER_EXPRTYPE_LT:
        return "LT";

    case MIDPARSER_EXPRTYPE_GT:
        return "GT";

    case MIDPARSER_EXPRTYPE_LTEQ:
        return "LTEQ";

    case MIDPARSER_EXPRTYPE_GTEQ:
        return "GTEQ";

    case MIDPARSER_EXPRTYPE_EQ:
        return "EQ";

    case MIDPARSER_EXPRTYPE_NEQ:
        return "NEQ";

    case MIDPARSER_EXPRTYPE_BITWISE_AND:
        return "BITWISE_AND";

    case MIDPARSER_EXPRTYPE_BITWISE_XOR:
        return "BITWISE_XOR";

    case MIDPARSER_EXPRTYPE_BITWISE_OR:
        return "BITWISE_OR";

    case MIDPARSER_EXPRTYPE_LOGICAL_AND:
        return "LOGICAL_AND";

    case MIDPARSER_EXPRTYPE_LOGICAL_OR:
        return "LOGICAL_OR";

    case MIDPARSER_EXPRTYPE_ASSIGN:
        return "ASSIGN";

    case MIDPARSER_EXPRTYPE_MUL_ASSIGN:
        return "MUL_ASSIGN";

    case MIDPARSER_EXPRTYPE_DIV_ASSIGN:
        return "DIV_ASSIGN";

    case MIDPARSER_EXPRTYPE_MOD_ASSIGN:
        return "MOD_ASSIGN";

    case MIDPARSER_EXPRTYPE_ADD_ASSIGN:
        return "ADD_ASSIGN";

    case MIDPARSER_EXPRTYPE_SUB_ASSIGN:
        return "SUB_ASSIGN";

    case MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        return "LEFT_SHIFT_ASSIGN";

    case MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        return "RIGHT_SHIFT_ASSIGN";

    case MIDPARSER_EXPRTYPE_AND_ASSIGN:
        return "AND_ASSIGN";

    case MIDPARSER_EXPRTYPE_OR_ASSIGN:
        return "OR_ASSIGN";

    case MIDPARSER_EXPRTYPE_XOR_ASSIGN:
        return "XOR_ASSIGN";

    case MIDPARSER_EXPRTYPE_COMMA:
        return "COMMA";

    case MIDPARSER_EXPRTYPE_BINOP_END:
        return "BINOP_END";

    case MIDPARSER_EXPRTYPE_UNARYOP_START:
        return "UNARYOP_START";

    case MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES:
        return "UNARY_SCOPE_RES";

    case MIDPARSER_EXPRTYPE_FUNC_CALL:
        return "FUNC_CALL";

    case MIDPARSER_EXPRTYPE_POSTFIX_INC:
        return "POSTFIX_INC";

    case MIDPARSER_EXPRTYPE_POSTFIX_DEC:
        return "POSTFIX_DEC";

    case MIDPARSER_EXPRTYPE_TYPEID:
        return "TYPEID";

    case MIDPARSER_EXPRTYPE_CONSTCAST:
        return "CONSTCAST";

    case MIDPARSER_EXPRTYPE_DYNAMICCAST:
        return "DYNAMICCAST";

    case MIDPARSER_EXPRTYPE_REINTERPRETCAST:
        return "REINTERPRETCAST";

    case MIDPARSER_EXPRTYPE_STATICCAST:
        return "STATICCAST";

    case MIDPARSER_EXPRTYPE_SIZEOF:
        return "SIZEOF";

    case MIDPARSER_EXPRTYPE_PREFIX_INC:
        return "PREFIX_INC";

    case MIDPARSER_EXPRTYPE_PREFIX_DEC:
        return "PREFIX_DEC";

    case MIDPARSER_EXPRTYPE_BITWISE_NOT:
        return "BITWISE_NOT";

    case MIDPARSER_EXPRTYPE_LOGICAL_NOT:
        return "LOGICAL_NOT";

    case MIDPARSER_EXPRTYPE_UNARY_PLUS:
        return "UNARY_PLUS";

    case MIDPARSER_EXPRTYPE_UNARY_MINUS:
        return "UNARY_MINUS";

    case MIDPARSER_EXPRTYPE_DEREF:
        return "DEREF";

    case MIDPARSER_EXPRTYPE_REF:
        return "REF";

    case MIDPARSER_EXPRTYPE_NEW:
        return "NEW";

    case MIDPARSER_EXPRTYPE_DELETE:
        return "DELETE";

    case MIDPARSER_EXPRTYPE_CAST:
        return "CAST";

    case MIDPARSER_EXPRTYPE_THROW:
        return "THROW";

    case MIDPARSER_EXPRTYPE_UNARYOP_END:
        return "UNARYOP_END";
    }
}
