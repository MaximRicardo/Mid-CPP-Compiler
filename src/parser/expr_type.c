#include "expr_type.h"

const char *Parser_exprtype_name(enum Parser_ExprType type)
{
    switch (type) {
    case PARSER_EXPRTYPE_NUMLIT_START:
        return "NUMLIT_START";

    case PARSER_EXPRTYPE_CHAR_LIT:
        return "CHAR_LIT";

    case PARSER_EXPRTYPE_WCHAR_LIT:
        return "WCHAR_LIT";

    case PARSER_EXPRTYPE_CHAR16_LIT:
        return "CHAR16_LIT";

    case PARSER_EXPRTYPE_CHAR32_LIT:
        return "CHAR32_LIT";

    case PARSER_EXPRTYPE_STRING_LIT:
        return "STRING_LIT";

    case PARSER_EXPRTYPE_WSTRING_LIT:
        return "WSTRING_LIT";

    case PARSER_EXPRTYPE_STRING16_LIT:
        return "STRING16_LIT";

    case PARSER_EXPRTYPE_STRING32_LIT:
        return "STRING32_LIT";

    case PARSER_EXPRTYPE_INT_LIT:
        return "INT_LIT";

    case PARSER_EXPRTYPE_UINT_LIT:
        return "UINT_LIT";

    case PARSER_EXPRTYPE_LONG_LIT:
        return "LONG_INT";

    case PARSER_EXPRTYPE_ULONG_LIT:
        return "ULONG_INT";

    case PARSER_EXPRTYPE_LONGLONG_LIT:
        return "LONGLONG_INT";

    case PARSER_EXPRTYPE_ULONGLONG_LIT:
        return "ULONGLONG_INT";

    case PARSER_EXPRTYPE_FLOAT_LIT:
        return "FLOAT_LIT";

    case PARSER_EXPRTYPE_DOUBLE_LIT:
        return "DOUBLE_LIT";

    case PARSER_EXPRTYPE_LONGDOUBLE_LIT:
        return "LONGDOUBLE_LIT";

    case PARSER_EXPRTYPE_BOOL_LIT:
        return "BOOL_LIT";

    case PARSER_EXPRTYPE_PTR_LIT:
        return "PTR_LIT";

    case PARSER_EXPRTYPE_NUMLIT_END:
        return "NUMLIT_END";

    case PARSER_EXPRTYPE_IDENTIFIER:
        return "IDENTIFIER";

    case PARSER_EXPRTYPE_THIS:
        return "THIS";

    case PARSER_EXPRTYPE_TERNARYOP_START:
        return "TERNARYOP_START";

    case PARSER_EXPRTYPE_CONDITIONAL:
        return "CONDITIONAL";

    case PARSER_EXPRTYPE_TERNARYOP_END:
        return "TERNARYOP_END";

    case PARSER_EXPRTYPE_BINOP_START:
        return "BINOP_START";

    case PARSER_EXPRTYPE_BIN_SCOPE_RES:
        return "BIN_SCOPE_RES";

    case PARSER_EXPRTYPE_MEMB_SEL:
        return "MEMB_SEL";

    case PARSER_EXPRTYPE_PTR_MEMB_SEL:
        return "PTR_MEMB_SEL";

    case PARSER_EXPRTYPE_ARRAY_SUBSCR:
        return "ARRAY_SUBSCR";

    case PARSER_EXPRTYPE_NEW_ARR:
        return "NEW_ARR";

    case PARSER_EXPRTYPE_DELETE_ARR:
        return "DELETE_ARR";

    case PARSER_EXPRTYPE_PTR_TO_MEMB_SEL:
        return "PTR_TO_MEMB_SEL";

    case PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        return "PTR_TO_PTR_MEMB_SEL";

    case PARSER_EXPRTYPE_MUL:
        return "MUL";

    case PARSER_EXPRTYPE_DIV:
        return "DIV";

    case PARSER_EXPRTYPE_MOD:
        return "MOD";

    case PARSER_EXPRTYPE_ADD:
        return "ADD";

    case PARSER_EXPRTYPE_SUB:
        return "SUB";

    case PARSER_EXPRTYPE_LEFT_SHIFT:
        return "LEFT_SHIFT";

    case PARSER_EXPRTYPE_RIGHT_SHIFT:
        return "RIGHT_SHIFT";

    case PARSER_EXPRTYPE_LT:
        return "LT";

    case PARSER_EXPRTYPE_GT:
        return "GT";

    case PARSER_EXPRTYPE_LTEQ:
        return "LTEQ";

    case PARSER_EXPRTYPE_GTEQ:
        return "GTEQ";

    case PARSER_EXPRTYPE_EQ:
        return "EQ";

    case PARSER_EXPRTYPE_NEQ:
        return "NEQ";

    case PARSER_EXPRTYPE_BITWISE_AND:
        return "BITWISE_AND";

    case PARSER_EXPRTYPE_BITWISE_XOR:
        return "BITWISE_XOR";

    case PARSER_EXPRTYPE_BITWISE_OR:
        return "BITWISE_OR";

    case PARSER_EXPRTYPE_LOGICAL_AND:
        return "LOGICAL_AND";

    case PARSER_EXPRTYPE_LOGICAL_OR:
        return "LOGICAL_OR";

    case PARSER_EXPRTYPE_ASSIGN:
        return "ASSIGN";

    case PARSER_EXPRTYPE_MUL_ASSIGN:
        return "MUL_ASSIGN";

    case PARSER_EXPRTYPE_DIV_ASSIGN:
        return "DIV_ASSIGN";

    case PARSER_EXPRTYPE_MOD_ASSIGN:
        return "MOD_ASSIGN";

    case PARSER_EXPRTYPE_ADD_ASSIGN:
        return "ADD_ASSIGN";

    case PARSER_EXPRTYPE_SUB_ASSIGN:
        return "SUB_ASSIGN";

    case PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        return "LEFT_SHIFT_ASSIGN";

    case PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        return "RIGHT_SHIFT_ASSIGN";

    case PARSER_EXPRTYPE_AND_ASSIGN:
        return "AND_ASSIGN";

    case PARSER_EXPRTYPE_OR_ASSIGN:
        return "OR_ASSIGN";

    case PARSER_EXPRTYPE_XOR_ASSIGN:
        return "XOR_ASSIGN";

    case PARSER_EXPRTYPE_COMMA:
        return "COMMA";

    case PARSER_EXPRTYPE_BINOP_END:
        return "BINOP_END";

    case PARSER_EXPRTYPE_UNARYOP_START:
        return "UNARYOP_START";

    case PARSER_EXPRTYPE_UNARY_SCOPE_RES:
        return "UNARY_SCOPE_RES";

    case PARSER_EXPRTYPE_FUNC_CALL:
        return "FUNC_CALL";

    case PARSER_EXPRTYPE_POSTFIX_INC:
        return "POSTFIX_INC";

    case PARSER_EXPRTYPE_POSTFIX_DEC:
        return "POSTFIX_DEC";

    case PARSER_EXPRTYPE_TYPEID:
        return "TYPEID";

    case PARSER_EXPRTYPE_CONSTCAST:
        return "CONSTCAST";

    case PARSER_EXPRTYPE_DYNAMICCAST:
        return "DYNAMICCAST";

    case PARSER_EXPRTYPE_REINTERPRETCAST:
        return "REINTERPRETCAST";

    case PARSER_EXPRTYPE_STATICCAST:
        return "STATICCAST";

    case PARSER_EXPRTYPE_SIZEOF:
        return "SIZEOF";

    case PARSER_EXPRTYPE_PREFIX_INC:
        return "PREFIX_INC";

    case PARSER_EXPRTYPE_PREFIX_DEC:
        return "PREFIX_DEC";

    case PARSER_EXPRTYPE_BITWISE_NOT:
        return "BITWISE_NOT";

    case PARSER_EXPRTYPE_LOGICAL_NOT:
        return "LOGICAL_NOT";

    case PARSER_EXPRTYPE_UNARY_PLUS:
        return "UNARY_PLUS";

    case PARSER_EXPRTYPE_UNARY_MINUS:
        return "UNARY_MINUS";

    case PARSER_EXPRTYPE_DEREF:
        return "DEREF";

    case PARSER_EXPRTYPE_REF:
        return "REF";

    case PARSER_EXPRTYPE_NEW:
        return "NEW";

    case PARSER_EXPRTYPE_DELETE:
        return "DELETE";

    case PARSER_EXPRTYPE_CAST:
        return "CAST";

    case PARSER_EXPRTYPE_THROW:
        return "THROW";

    case PARSER_EXPRTYPE_UNARYOP_END:
        return "UNARYOP_END";
    }
}
