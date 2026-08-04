#include "parser/expr_type.h"

const char *midpar_exprtype_name(enum midpar_ExprType type)
{
    switch (type) {
    case MIDPAR_EXPRTYPE_NUMLIT_START:
        return "NUMLIT_START";

    case MIDPAR_EXPRTYPE_CHAR_LIT:
        return "CHAR_LIT";

    case MIDPAR_EXPRTYPE_WCHAR_LIT:
        return "WCHAR_LIT";

    case MIDPAR_EXPRTYPE_CHAR16_LIT:
        return "CHAR16_LIT";

    case MIDPAR_EXPRTYPE_CHAR32_LIT:
        return "CHAR32_LIT";

    case MIDPAR_EXPRTYPE_STRING_LIT:
        return "STRING_LIT";

    case MIDPAR_EXPRTYPE_WSTRING_LIT:
        return "WSTRING_LIT";

    case MIDPAR_EXPRTYPE_STRING16_LIT:
        return "STRING16_LIT";

    case MIDPAR_EXPRTYPE_STRING32_LIT:
        return "STRING32_LIT";

    case MIDPAR_EXPRTYPE_INT_LIT:
        return "INT_LIT";

    case MIDPAR_EXPRTYPE_UINT_LIT:
        return "UINT_LIT";

    case MIDPAR_EXPRTYPE_LONG_LIT:
        return "LONG_INT";

    case MIDPAR_EXPRTYPE_ULONG_LIT:
        return "ULONG_INT";

    case MIDPAR_EXPRTYPE_LONGLONG_LIT:
        return "LONGLONG_INT";

    case MIDPAR_EXPRTYPE_ULONGLONG_LIT:
        return "ULONGLONG_INT";

    case MIDPAR_EXPRTYPE_FLTLIT_START:
        return "FLTLIT_START";

    case MIDPAR_EXPRTYPE_FLOAT_LIT:
        return "FLOAT_LIT";

    case MIDPAR_EXPRTYPE_DOUBLE_LIT:
        return "DOUBLE_LIT";

    case MIDPAR_EXPRTYPE_LONGDOUBLE_LIT:
        return "LONGDOUBLE_LIT";

    case MIDPAR_EXPRTYPE_FLTLIT_END:
        return "FLTLIT_END";

    case MIDPAR_EXPRTYPE_BOOL_LIT:
        return "BOOL_LIT";

    case MIDPAR_EXPRTYPE_NULLPTR_LIT:
        return "NULLPTR_LIT";

    case MIDPAR_EXPRTYPE_NUMLIT_END:
        return "NUMLIT_END";

    case MIDPAR_EXPRTYPE_IDENTIFIER:
        return "IDENTIFIER";

    case MIDPAR_EXPRTYPE_THIS:
        return "THIS";

    case MIDPAR_EXPRTYPE_TERNARYOP_START:
        return "TERNARYOP_START";

    case MIDPAR_EXPRTYPE_CONDITIONAL:
        return "CONDITIONAL";

    case MIDPAR_EXPRTYPE_TERNARYOP_END:
        return "TERNARYOP_END";

    case MIDPAR_EXPRTYPE_BINOP_START:
        return "BINOP_START";

    case MIDPAR_EXPRTYPE_BIN_SCOPE_RES:
        return "BIN_SCOPE_RES";

    case MIDPAR_EXPRTYPE_MEMB_SEL:
        return "MEMB_SEL";

    case MIDPAR_EXPRTYPE_PTR_MEMB_SEL:
        return "PTR_MEMB_SEL";

    case MIDPAR_EXPRTYPE_ARRAY_SUBSCR:
        return "ARRAY_SUBSCR";

    case MIDPAR_EXPRTYPE_NEW_ARR:
        return "NEW_ARR";

    case MIDPAR_EXPRTYPE_DELETE_ARR:
        return "DELETE_ARR";

    case MIDPAR_EXPRTYPE_PTR_TO_MEMB_SEL:
        return "PTR_TO_MEMB_SEL";

    case MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        return "PTR_TO_PTR_MEMB_SEL";

    case MIDPAR_EXPRTYPE_MUL:
        return "MUL";

    case MIDPAR_EXPRTYPE_DIV:
        return "DIV";

    case MIDPAR_EXPRTYPE_MOD:
        return "MOD";

    case MIDPAR_EXPRTYPE_ADD:
        return "ADD";

    case MIDPAR_EXPRTYPE_SUB:
        return "SUB";

    case MIDPAR_EXPRTYPE_LEFT_SHIFT:
        return "LEFT_SHIFT";

    case MIDPAR_EXPRTYPE_RIGHT_SHIFT:
        return "RIGHT_SHIFT";

    case MIDPAR_EXPRTYPE_LT:
        return "LT";

    case MIDPAR_EXPRTYPE_GT:
        return "GT";

    case MIDPAR_EXPRTYPE_LTEQ:
        return "LTEQ";

    case MIDPAR_EXPRTYPE_GTEQ:
        return "GTEQ";

    case MIDPAR_EXPRTYPE_EQ:
        return "EQ";

    case MIDPAR_EXPRTYPE_NEQ:
        return "NEQ";

    case MIDPAR_EXPRTYPE_BITWISE_AND:
        return "BITWISE_AND";

    case MIDPAR_EXPRTYPE_BITWISE_XOR:
        return "BITWISE_XOR";

    case MIDPAR_EXPRTYPE_BITWISE_OR:
        return "BITWISE_OR";

    case MIDPAR_EXPRTYPE_LOGICAL_AND:
        return "LOGICAL_AND";

    case MIDPAR_EXPRTYPE_LOGICAL_OR:
        return "LOGICAL_OR";

    case MIDPAR_EXPRTYPE_ASSIGN:
        return "ASSIGN";

    case MIDPAR_EXPRTYPE_MUL_ASSIGN:
        return "MUL_ASSIGN";

    case MIDPAR_EXPRTYPE_DIV_ASSIGN:
        return "DIV_ASSIGN";

    case MIDPAR_EXPRTYPE_MOD_ASSIGN:
        return "MOD_ASSIGN";

    case MIDPAR_EXPRTYPE_ADD_ASSIGN:
        return "ADD_ASSIGN";

    case MIDPAR_EXPRTYPE_SUB_ASSIGN:
        return "SUB_ASSIGN";

    case MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        return "LEFT_SHIFT_ASSIGN";

    case MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        return "RIGHT_SHIFT_ASSIGN";

    case MIDPAR_EXPRTYPE_AND_ASSIGN:
        return "AND_ASSIGN";

    case MIDPAR_EXPRTYPE_OR_ASSIGN:
        return "OR_ASSIGN";

    case MIDPAR_EXPRTYPE_XOR_ASSIGN:
        return "XOR_ASSIGN";

    case MIDPAR_EXPRTYPE_COMMA:
        return "COMMA";

    case MIDPAR_EXPRTYPE_BINOP_END:
        return "BINOP_END";

    case MIDPAR_EXPRTYPE_UNARYOP_START:
        return "UNARYOP_START";

    case MIDPAR_EXPRTYPE_UNARY_SCOPE_RES:
        return "UNARY_SCOPE_RES";

    case MIDPAR_EXPRTYPE_FUNC_CALL:
        return "FUNC_CALL";

    case MIDPAR_EXPRTYPE_POSTFIX_INC:
        return "POSTFIX_INC";

    case MIDPAR_EXPRTYPE_POSTFIX_DEC:
        return "POSTFIX_DEC";

    case MIDPAR_EXPRTYPE_TYPEID:
        return "TYPEID";

    case MIDPAR_EXPRTYPE_CONSTCAST:
        return "CONSTCAST";

    case MIDPAR_EXPRTYPE_DYNAMICCAST:
        return "DYNAMICCAST";

    case MIDPAR_EXPRTYPE_REINTERPRETCAST:
        return "REINTERPRETCAST";

    case MIDPAR_EXPRTYPE_STATICCAST:
        return "STATICCAST";

    case MIDPAR_EXPRTYPE_SIZEOF:
        return "SIZEOF";

    case MIDPAR_EXPRTYPE_PREFIX_INC:
        return "PREFIX_INC";

    case MIDPAR_EXPRTYPE_PREFIX_DEC:
        return "PREFIX_DEC";

    case MIDPAR_EXPRTYPE_BITWISE_NOT:
        return "BITWISE_NOT";

    case MIDPAR_EXPRTYPE_LOGICAL_NOT:
        return "LOGICAL_NOT";

    case MIDPAR_EXPRTYPE_UNARY_PLUS:
        return "UNARY_PLUS";

    case MIDPAR_EXPRTYPE_UNARY_MINUS:
        return "UNARY_MINUS";

    case MIDPAR_EXPRTYPE_DEREF:
        return "DEREF";

    case MIDPAR_EXPRTYPE_REF:
        return "REF";

    case MIDPAR_EXPRTYPE_NEW:
        return "NEW";

    case MIDPAR_EXPRTYPE_DELETE:
        return "DELETE";

    case MIDPAR_EXPRTYPE_CAST:
        return "CAST";

    case MIDPAR_EXPRTYPE_THROW:
        return "THROW";

    case MIDPAR_EXPRTYPE_UNARYOP_END:
        return "UNARYOP_END";
    }
}
