#include "parser/expr.h"
#include "apfloat.h"
#include "apint.h"
#include "cmd.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "parser/end_types.h"
#include "parser/expr_type.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <assert.h>
#include <stdio.h>

bool midpar_is_strlit(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_STRING_LIT ||
           type == MIDPAR_EXPRTYPE_WSTRING_LIT ||
           type == MIDPAR_EXPRTYPE_STRING16_LIT ||
           type == MIDPAR_EXPRTYPE_STRING32_LIT;
}

bool midpar_is_fltlit(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_FLTLIT_START &&
           type < MIDPAR_EXPRTYPE_FLTLIT_END;
}

bool midpar_is_intlit(enum midpar_ExprType type)
{
    return midpar_is_numlit(type) && !midpar_is_fltlit(type);
}

bool midpar_is_numlit(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_NUMLIT_START &&
           type < MIDPAR_EXPRTYPE_NUMLIT_END;
}

bool midpar_is_ternaryop(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_TERNARYOP_START &&
           type < MIDPAR_EXPRTYPE_TERNARYOP_END;
}

bool midpar_is_binop(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_BINOP_START &&
           type < MIDPAR_EXPRTYPE_BINOP_END;
}

bool midpar_is_unaryop(enum midpar_ExprType type)
{
    return type > MIDPAR_EXPRTYPE_UNARYOP_START &&
           type < MIDPAR_EXPRTYPE_UNARYOP_END;
}

bool midpar_is_scope_res(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_BIN_SCOPE_RES ||
           type == MIDPAR_EXPRTYPE_UNARY_SCOPE_RES;
}

bool midpar_is_op(enum midpar_ExprType type)
{
    return midpar_is_binop(type) || midpar_is_unaryop(type);
}

bool midpar_is_arith_op(enum midpar_ExprType type)
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

bool midpar_is_logical_op(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_LOGICAL_AND ||
           type == MIDPAR_EXPRTYPE_LOGICAL_OR ||
           type == MIDPAR_EXPRTYPE_LOGICAL_NOT;
}

bool midpar_is_comp_op(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_LT || type == MIDPAR_EXPRTYPE_GT ||
           type == MIDPAR_EXPRTYPE_LTEQ || type == MIDPAR_EXPRTYPE_GTEQ ||
           type == MIDPAR_EXPRTYPE_EQ || type == MIDPAR_EXPRTYPE_NEQ;
}

bool midpar_is_assignment(enum midpar_ExprType type)
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

bool midpar_is_memb_sel(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_TO_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
}

int32_t midpar_op_precedence(enum midpar_ExprType op)
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

bool midpar_op_ltr_assoc(enum midpar_ExprType op)
{
    int32_t prec = midpar_op_precedence(op);
    return prec != 15 && prec != 13 && prec != 1;
}

bool midpar_is_glvalue(enum midpar_ExprValueType type)
{
    return type == MIDPAR_EXPRVALUE_LVALUE || type == MIDPAR_EXPRVALUE_XVALUE;
}

bool midpar_is_rvalue(enum midpar_ExprValueType type)
{
    return type == MIDPAR_EXPRVALUE_PRVALUE || type == MIDPAR_EXPRVALUE_XVALUE;
}

bool midpar_is_rvalue(enum midpar_ExprValueType type);

static struct midpar_Expr lit_tok_to_expr(const struct midlex_Token *tok)
{
    assert(midlex_is_lit(tok->type));

    struct midpar_Expr ret = {
        .tok = tok, .info.val = tok->val, .valtype = MIDPAR_EXPRVALUE_PRVALUE};

    switch (tok->type) {
    case MIDLEX_TOKENTYPE_CHAR_LIT:
        ret.type = MIDPAR_EXPRTYPE_CHAR_LIT;
        break;

    case MIDLEX_TOKENTYPE_WCHAR_LIT:
        ret.type = MIDPAR_EXPRTYPE_WCHAR_LIT;
        break;

    case MIDLEX_TOKENTYPE_CHAR16_LIT:
        ret.type = MIDPAR_EXPRTYPE_CHAR16_LIT;
        break;

    case MIDLEX_TOKENTYPE_CHAR32_LIT:
        ret.type = MIDPAR_EXPRTYPE_CHAR32_LIT;
        break;

    case MIDLEX_TOKENTYPE_STRING_LIT:
        ret.type = MIDPAR_EXPRTYPE_STRING_LIT;
        break;

    case MIDLEX_TOKENTYPE_WSTRING_LIT:
        ret.type = MIDPAR_EXPRTYPE_WSTRING_LIT;
        break;

    case MIDLEX_TOKENTYPE_STRING16_LIT:
        ret.type = MIDPAR_EXPRTYPE_STRING16_LIT;
        break;

    case MIDLEX_TOKENTYPE_STRING32_LIT:
        ret.type = MIDPAR_EXPRTYPE_STRING32_LIT;
        break;

    case MIDLEX_TOKENTYPE_INT_LIT:
        ret.type = MIDPAR_EXPRTYPE_INT_LIT;
        break;

    case MIDLEX_TOKENTYPE_UINT_LIT:
        ret.type = MIDPAR_EXPRTYPE_UINT_LIT;
        break;

    case MIDLEX_TOKENTYPE_LONG_LIT:
        ret.type = MIDPAR_EXPRTYPE_LONG_LIT;
        break;

    case MIDLEX_TOKENTYPE_ULONG_LIT:
        ret.type = MIDPAR_EXPRTYPE_ULONG_LIT;
        break;

    case MIDLEX_TOKENTYPE_LONGLONG_LIT:
        ret.type = MIDPAR_EXPRTYPE_LONGLONG_LIT;
        break;

    case MIDLEX_TOKENTYPE_ULONGLONG_LIT:
        ret.type = MIDPAR_EXPRTYPE_ULONGLONG_LIT;
        break;

    case MIDLEX_TOKENTYPE_FLOAT_LIT:
        ret.type = MIDPAR_EXPRTYPE_FLOAT_LIT;
        break;

    case MIDLEX_TOKENTYPE_DOUBLE_LIT:
        ret.type = MIDPAR_EXPRTYPE_DOUBLE_LIT;
        break;

    case MIDLEX_TOKENTYPE_LONGDOUBLE_LIT:
        ret.type = MIDPAR_EXPRTYPE_LONGDOUBLE_LIT;
        break;

    case MIDLEX_TOKENTYPE_BOOL_LIT:
        ret.type = MIDPAR_EXPRTYPE_BOOL_LIT;
        break;

    case MIDLEX_TOKENTYPE_NULLPTR_LIT:
        ret.type = MIDPAR_EXPRTYPE_NULLPTR_LIT;
        break;

    default:
        MID_CRASH("token is not literal");
    }

    return ret;
}

static struct midpar_Expr ident_tok_to_expr(const struct midlex_Token *tok)
{
    assert(tok->type == MIDLEX_TOKENTYPE_IDENTIFIER);

    struct midpar_Expr ret = {.tok = tok,
                              .info.ident = tok->ident,
                              .type = MIDPAR_EXPRTYPE_IDENTIFIER};
    return ret;
}

static struct midpar_Expr this_tok_to_expr(const struct midlex_Token *tok)
{
    struct midpar_Expr ret = {
        .tok = tok, .info.ident = tok->ident, .type = MIDPAR_EXPRTYPE_THIS};
    return ret;
}

static struct midpar_Expr op_tok_to_expr_mode0(const struct midlex_Token *tok,
                                               struct mid_DiagVec *diags)
{
    struct midpar_Expr ret = {.tok = tok};

    switch (tok->type) {
    case MIDLEX_TOKENTYPE_SCOPE_RES:
        ret.type = MIDPAR_EXPRTYPE_BIN_SCOPE_RES;
        break;

    case MIDLEX_TOKENTYPE_MEMB_SEL:
        ret.type = MIDPAR_EXPRTYPE_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_PTR_MEMB_SEL:
        ret.type = MIDPAR_EXPRTYPE_PTR_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_L_SQBRACKET:
        ret.type = MIDPAR_EXPRTYPE_ARRAY_SUBSCR;
        break;

    case MIDLEX_TOKENTYPE_L_PAREN:
        ret.type = MIDPAR_EXPRTYPE_FUNC_CALL;
        break;

    case MIDLEX_TOKENTYPE_INC:
        ret.type = MIDPAR_EXPRTYPE_POSTFIX_INC;
        break;

    case MIDLEX_TOKENTYPE_DEC:
        ret.type = MIDPAR_EXPRTYPE_POSTFIX_DEC;
        break;

    case MIDLEX_TOKENTYPE_TYPEID:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "typeid", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_TYPEID;
        break;

    case MIDLEX_TOKENTYPE_CONSTCAST:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("const_cast", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_CONSTCAST;
        break;

    case MIDLEX_TOKENTYPE_DYNAMICCAST:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("dynamic_cast", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_DYNAMICCAST;
        break;

    case MIDLEX_TOKENTYPE_REINTERPRETCAST:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("reinterpret_cast", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_REINTERPRETCAST;
        break;

    case MIDLEX_TOKENTYPE_STATICCAST:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("static_cast", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_STATICCAST;
        break;

    case MIDLEX_TOKENTYPE_SIZEOF:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "sizeof", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_SIZEOF;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_NOT:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("bitwise NOT '~'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_BITWISE_NOT;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_NOT:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("logical NOT '!'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LOGICAL_NOT;
        break;

    case MIDLEX_TOKENTYPE_SUB:
        ret.type = MIDPAR_EXPRTYPE_SUB;
        break;

    case MIDLEX_TOKENTYPE_ADD:
        ret.type = MIDPAR_EXPRTYPE_ADD;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_AND:
        ret.type = MIDPAR_EXPRTYPE_BITWISE_AND;
        break;

    case MIDLEX_TOKENTYPE_MUL:
        ret.type = MIDPAR_EXPRTYPE_MUL;
        break;

    case MIDLEX_TOKENTYPE_NEW:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "new", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_NEW;
        break;

    case MIDLEX_TOKENTYPE_DELETE:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "delete", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_DELETE;
        break;

    case MIDLEX_TOKENTYPE_PTR_TO_MEMB_SEL:
        ret.type = MIDPAR_EXPRTYPE_PTR_TO_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        ret.type = MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_DIV:
        ret.type = MIDPAR_EXPRTYPE_DIV;
        break;

    case MIDLEX_TOKENTYPE_MOD:
        ret.type = MIDPAR_EXPRTYPE_MOD;
        break;

    case MIDLEX_TOKENTYPE_LEFT_SHIFT:
        ret.type = MIDPAR_EXPRTYPE_LEFT_SHIFT;
        break;

    case MIDLEX_TOKENTYPE_RIGHT_SHIFT:
        ret.type = MIDPAR_EXPRTYPE_RIGHT_SHIFT;
        break;

    case MIDLEX_TOKENTYPE_LT:
        ret.type = MIDPAR_EXPRTYPE_LT;
        break;

    case MIDLEX_TOKENTYPE_GT:
        ret.type = MIDPAR_EXPRTYPE_GT;
        break;

    case MIDLEX_TOKENTYPE_LTEQ:
        ret.type = MIDPAR_EXPRTYPE_LTEQ;
        break;

    case MIDLEX_TOKENTYPE_GTEQ:
        ret.type = MIDPAR_EXPRTYPE_GTEQ;
        break;

    case MIDLEX_TOKENTYPE_EQ:
        ret.type = MIDPAR_EXPRTYPE_EQ;
        break;

    case MIDLEX_TOKENTYPE_NEQ:
        ret.type = MIDPAR_EXPRTYPE_NEQ;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_XOR:
        ret.type = MIDPAR_EXPRTYPE_BITWISE_XOR;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_OR:
        ret.type = MIDPAR_EXPRTYPE_BITWISE_OR;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_AND:
        ret.type = MIDPAR_EXPRTYPE_LOGICAL_AND;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_OR:
        ret.type = MIDPAR_EXPRTYPE_LOGICAL_OR;
        break;

    case MIDLEX_TOKENTYPE_CONDITIONAL:
        ret.type = MIDPAR_EXPRTYPE_CONDITIONAL;
        break;

    case MIDLEX_TOKENTYPE_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_MUL_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_MUL_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_DIV_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_DIV_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_MOD_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_MOD_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_ADD_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_ADD_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_SUB_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_SUB_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_AND_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_AND_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_OR_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_OR_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_XOR_ASSIGN:
        ret.type = MIDPAR_EXPRTYPE_XOR_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_THROW:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "throw", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_THROW;
        break;

    case MIDLEX_TOKENTYPE_COMMA:
        ret.type = MIDPAR_EXPRTYPE_COMMA;
        break;

    default:
        MID_CRASH("can't convert token to expr");
    }

    return ret;
}

static struct midpar_Expr op_tok_to_expr_mode1(const struct midlex_Token *tok,
                                               struct mid_DiagVec *diags)
{
    struct midpar_Expr ret = {.tok = tok};

    switch (tok->type) {
    case MIDLEX_TOKENTYPE_SCOPE_RES:
        ret.type = MIDPAR_EXPRTYPE_UNARY_SCOPE_RES;
        break;

    case MIDLEX_TOKENTYPE_MEMB_SEL:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("member select '.'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_PTR_MEMB_SEL:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "ptr to member select '->'", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_PTR_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_L_SQBRACKET:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("array subscript '[]'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_ARRAY_SUBSCR;
        break;

    case MIDLEX_TOKENTYPE_L_PAREN:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("function call '()'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_FUNC_CALL;
        break;

    case MIDLEX_TOKENTYPE_INC:
        ret.type = MIDPAR_EXPRTYPE_PREFIX_INC;
        break;

    case MIDLEX_TOKENTYPE_DEC:
        ret.type = MIDPAR_EXPRTYPE_PREFIX_DEC;
        break;

    case MIDLEX_TOKENTYPE_TYPEID:
        ret.type = MIDPAR_EXPRTYPE_TYPEID;
        break;

    case MIDLEX_TOKENTYPE_CONSTCAST:
        ret.type = MIDPAR_EXPRTYPE_CONSTCAST;
        break;

    case MIDLEX_TOKENTYPE_DYNAMICCAST:
        ret.type = MIDPAR_EXPRTYPE_DYNAMICCAST;
        break;

    case MIDLEX_TOKENTYPE_REINTERPRETCAST:
        ret.type = MIDPAR_EXPRTYPE_REINTERPRETCAST;
        break;

    case MIDLEX_TOKENTYPE_STATICCAST:
        ret.type = MIDPAR_EXPRTYPE_STATICCAST;
        break;

    case MIDLEX_TOKENTYPE_SIZEOF:
        ret.type = MIDPAR_EXPRTYPE_SIZEOF;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_NOT:
        ret.type = MIDPAR_EXPRTYPE_BITWISE_NOT;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_NOT:
        ret.type = MIDPAR_EXPRTYPE_LOGICAL_NOT;
        break;

    case MIDLEX_TOKENTYPE_SUB:
        ret.type = MIDPAR_EXPRTYPE_UNARY_MINUS;
        break;

    case MIDLEX_TOKENTYPE_ADD:
        ret.type = MIDPAR_EXPRTYPE_UNARY_PLUS;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_AND:
        ret.type = MIDPAR_EXPRTYPE_REF;
        break;

    case MIDLEX_TOKENTYPE_MUL:
        ret.type = MIDPAR_EXPRTYPE_DEREF;
        break;

    case MIDLEX_TOKENTYPE_NEW:
        ret.type = MIDPAR_EXPRTYPE_NEW;
        break;

    case MIDLEX_TOKENTYPE_DELETE:
        ret.type = MIDPAR_EXPRTYPE_DELETE;
        break;

    case MIDLEX_TOKENTYPE_PTR_TO_MEMB_SEL:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "ptr to member select '.*'", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_PTR_TO_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "ptr to ptr member select '->*'", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
        break;

    case MIDLEX_TOKENTYPE_DIV:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("division '/'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_DIV;
        break;

    case MIDLEX_TOKENTYPE_MOD:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("modulo '%'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_MOD;
        break;

    case MIDLEX_TOKENTYPE_LEFT_SHIFT:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("left shift '<<'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LEFT_SHIFT;
        break;

    case MIDLEX_TOKENTYPE_RIGHT_SHIFT:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("right shift '>>'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_RIGHT_SHIFT;
        break;

    case MIDLEX_TOKENTYPE_LT:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("less than '<'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LT;
        break;

    case MIDLEX_TOKENTYPE_GT:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("greater than '>'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_GT;
        break;

    case MIDLEX_TOKENTYPE_LTEQ:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("less than or equal '<='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LTEQ;
        break;

    case MIDLEX_TOKENTYPE_GTEQ:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "greater than or equal '>='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_GTEQ;
        break;

    case MIDLEX_TOKENTYPE_EQ:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("equality '=='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_EQ;
        break;

    case MIDLEX_TOKENTYPE_NEQ:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("inequality '!='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_NEQ;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_XOR:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("bitwise XOR '^'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_BITWISE_XOR;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_OR:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("bitwise OR '|'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_BITWISE_OR;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_AND:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("logical AND '&&'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LOGICAL_AND;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_OR:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("logical OR '||'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LOGICAL_OR;
        break;

    case MIDLEX_TOKENTYPE_CONDITIONAL:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("conditional operator '?'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_CONDITIONAL;
        break;

    case MIDLEX_TOKENTYPE_ASSIGN:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("assignment '=='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_MUL_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "multiplication assignment '*='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_MUL_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_DIV_ASSIGN:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("division assignment '/='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_DIV_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_MOD_ASSIGN:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("modulus assignment '%='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_MOD_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_ADD_ASSIGN:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("addition assignment '+='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_ADD_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_SUB_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "subtraction assignment '-='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_SUB_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "left shift assignment '<<='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "right shift assignment '>>='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_AND_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "bitwise AND assignment '&='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_AND_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_OR_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "bitwise OR assignment '|='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_OR_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_XOR_ASSIGN:
        midgen_dynpush(diags, middiag_unexpected_token_err(
                                  "bitwise XOR assignment '^='", tok,
                                  MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_XOR_ASSIGN;
        break;

    case MIDLEX_TOKENTYPE_THROW:
        ret.type = MIDPAR_EXPRTYPE_THROW;
        break;

    case MIDLEX_TOKENTYPE_COMMA:
        midgen_dynpush(
            diags, middiag_unexpected_token_err("comma ','", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPAR_EXPRTYPE_COMMA;
        break;

    default:
        MID_CRASH("can't convert token to expr");
    }

    return ret;
}

static void parse_func_call_args(struct midpar_Expr *f_call,
                                 const struct midlex_Token *toks,
                                 mid_isize lparen, mid_isize *out_rparen,
                                 struct midsema_Scope *scope,
                                 struct mid_DiagVec *diags)
{
    mid_isize rparen = midpar_find_twin_paren(toks, lparen, MID_ISIZE_MAX);
    if (rparen == -1) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("')'", &toks[lparen],
                                                  MIDDIAG_ERR_MISSING_PAREN));
        rparen = lparen;
    }
    if (out_rparen)
        *out_rparen = rparen;

    for (mid_isize i = lparen + 1; i < rparen; ++i) {
        auto arg =
            midpar_parse_expr(toks, i, MIDPAR_ARG_ENDTYPES, &i, scope, diags);
        midgen_dynpush(&f_call->info.args, arg);

        if (toks[i].type != MIDLEX_TOKENTYPE_R_PAREN &&
            toks[i].type != MIDLEX_TOKENTYPE_COMMA) {
            midgen_dynpush(
                diags, middiag_expected_token_err("','", &toks[lparen],
                                                  MIDDIAG_ERR_MISSING_PAREN));
        }
    }
}

// in most cases out_end_idx is set to idx, but in some cases like func calls
// and array subscripts out_end_idx is set to the last token in the expr:
// func(a, b, c, d)
//     ^          ^
//    idx    out_end_idx
static struct midpar_Expr op_tok_to_expr(const struct midlex_Token *toks,
                                         mid_isize idx, mid_isize *out_end_idx,
                                         bool mode, struct midsema_Scope *scope,
                                         struct mid_DiagVec *diags)
{
    struct midpar_Expr ret = mode ? op_tok_to_expr_mode1(&toks[idx], diags)
                                  : op_tok_to_expr_mode0(&toks[idx], diags);

    if (ret.type == MIDPAR_EXPRTYPE_FUNC_CALL)
        parse_func_call_args(&ret, toks, idx, out_end_idx, scope, diags);
    else if (out_end_idx)
        *out_end_idx = idx;

    return ret;
}

static bool has_enough_operands(enum midpar_ExprType op, int n)
{
    if (midpar_is_unaryop(op))
        return n >= 1;
    else if (midpar_is_binop(op))
        return n >= 2;
    else if (midpar_is_ternaryop(op))
        return n >= 3;
    else
        MID_CRASH("bad expression type");
}

static void add_op_to_out(struct midpar_Expr *op, struct midpar_ExprVec *out,
                          struct mid_DiagVec *diags)
{
    if (!has_enough_operands(op->type, out->len)) {
        struct mid_Diag err = {
            .pos = op->tok->pos,
            .line = op->tok->line,
            .msg = midcmd_fmt_to_str(
                "%s operator expects %d %s, received %" MID_PRIisz,
                midpar_is_unaryop(op->type) ? "unary"
                : midpar_is_binop(op->type) ? "binary"
                                            : "ternary",
                midpar_is_unaryop(op->type) ? 1
                : midpar_is_binop(op->type) ? 2
                                            : 3,
                midpar_is_unaryop(op->type) ? "operand" : "operands", out->len),
            .err = MIDDIAG_ERR_INSUFFICIENT_OPERANDS,
            .type = MIDDIAG_TYPE_ERROR};
        midgen_dynpush(diags, err);
        return;
    }

    // the exprs at the top act as operands for the new op
    if (midpar_is_ternaryop(op->type))
        midgen_dynpush(&op->info.args, out->arr[out->len - 3]);
    if (midpar_is_ternaryop(op->type) || midpar_is_binop(op->type))
        midgen_dynpush(&op->info.args, out->arr[out->len - 2]);
    if (op->type == MIDPAR_EXPRTYPE_FUNC_CALL && op->info.args.len > 0)
        // func calls already have the arguments pushed into args, so we gotta
        // use insert to put the identifier being called first
        midgen_dyninsert(&op->info.args, 0, out->arr[out->len - 1]);
    else
        midgen_dynpush(&op->info.args, out->arr[out->len - 1]);

    // the expressions are now encoded in op
    if (midpar_is_ternaryop(op->type))
        midgen_dynpop(out);
    if (midpar_is_ternaryop(op->type) || midpar_is_binop(op->type))
        midgen_dynpop(out);
    midgen_dynpop(out);

    midgen_dynpush(out, *op);
}

// handles sending an operator through the shunting yard
static void push_operator(const struct midlex_Token *toks, mid_isize idx,
                          mid_isize *out_end_idx, struct midpar_ExprVec *out,
                          struct midpar_ExprVec *ops, bool mode,
                          struct midsema_Scope *scope,
                          struct mid_DiagVec *diags)
{
    struct midpar_Expr op =
        op_tok_to_expr(toks, idx, out_end_idx, mode, scope, diags);

    // remove any greater precedence operators
    struct midpar_Expr *top = &ops->arr[ops->len - 1];
    while (ops->len > 0) {
        int32_t op_prec = midpar_op_precedence(op.type);
        int32_t top_prec = midpar_op_precedence(top->type);

        if (top_prec > op_prec ||
            (top_prec == op_prec && midpar_op_ltr_assoc(op.type))) {
            add_op_to_out(top, out, diags);
            midgen_dynpop(ops);
            top = &ops->arr[ops->len - 1];
        } else {
            break;
        }
    }

    midgen_dynpush(ops, op);
}

// a sub expression is a part of an expression encased in parentheses
static struct midpar_Expr parse_subexpr(const struct midlex_Token *toks,
                                        mid_isize l_paren, mid_isize *out_end,
                                        struct midsema_Scope *scope,
                                        struct mid_DiagVec *diags)
{
    if (midpar_find_twin_paren(toks, l_paren, MID_ISIZE_MAX) == -1) {
        struct mid_Diag err = {.pos = toks[l_paren].pos,
                               .line = toks[l_paren].line,
                               .msg = midcmd_fmt_to_str("expected ')'"),
                               .err = MIDDIAG_ERR_MISSING_PAREN,
                               .type = MIDDIAG_TYPE_ERROR};
        midgen_dynpush(diags, err);
    }

    return midpar_parse_expr(
        toks, l_paren + 1, (enum midlex_TokenType[]){MIDLEX_TOKENTYPE_R_PAREN},
        1, out_end, scope, diags);
}

static bool is_end_type(enum midlex_TokenType type,
                        const enum midlex_TokenType *end_types,
                        mid_isize n_end_types)
{
    for (mid_isize i = 0; i < n_end_types; ++i)
        if (type == end_types[i])
            return true;
    return false;
}

struct midpar_Expr midpar_parse_expr(const struct midlex_Token *toks,
                                     mid_isize start,
                                     const enum midlex_TokenType *end_types,
                                     mid_isize n_end_types, mid_isize *out_end,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags)
{
    // uses the shunting yard algorithm

    struct midpar_ExprVec out = midgen_dyninit();
    struct midpar_ExprVec ops = midgen_dyninit();

    // when false, binary operators remain binary and unary operators are
    // treated as postifx operators
    // when true, binary operators are treated as unary and unary operators are
    // treated as prefix operators
    // becomes true after finding an operand, becomes false after finding an
    // operator unless it's a unary postfix operator
    bool mode = true;

    mid_isize i;
    for (i = start; !is_end_type(toks[i].type, end_types, n_end_types); ++i) {
        if (midlex_is_lit(toks[i].type)) {
            if (!mode)
                midgen_dynpush(diags, middiag_unexpected_token_err(
                                          "literal", &toks[i],
                                          MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                midgen_dynpush(&out, lit_tok_to_expr(&toks[i]));
            mode = false;
        } else if (toks[i].type == MIDLEX_TOKENTYPE_IDENTIFIER) {
            if (!mode)
                midgen_dynpush(diags, middiag_unexpected_token_err(
                                          "identifier", &toks[i],
                                          MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                midgen_dynpush(&out, ident_tok_to_expr(&toks[i]));
            mode = false;
        } else if (toks[i].type == MIDLEX_TOKENTYPE_THIS) {
            if (!mode)
                midgen_dynpush(
                    diags, middiag_unexpected_token_err(
                               "this", &toks[i], MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                midgen_dynpush(&out, this_tok_to_expr(&toks[i]));
            mode = false;
        } else if (midlex_is_op(toks[i].type)) {
            push_operator(toks, i, &i, &out, &ops, mode, scope, diags);
            mode = ops.arr[ops.len - 1].type != MIDPAR_EXPRTYPE_POSTFIX_DEC &&
                   ops.arr[ops.len - 1].type != MIDPAR_EXPRTYPE_POSTFIX_INC;
        } else if (toks[i].type == MIDLEX_TOKENTYPE_L_PAREN) {
            if (!mode)
                // if mode is 0, a sub-expression is actually a function call
                push_operator(toks, i, &i, &out, &ops, mode, scope, diags);
            else
                midgen_dynpush(&out, parse_subexpr(toks, i, &i, scope, diags));
            mode = false;
        }
    }
    if (out_end)
        *out_end = i;

    // excess operators just get popped in fifo order
    while (ops.len > 0) {
        add_op_to_out(&ops.arr[ops.len - 1], &out, diags);
        midgen_dynpop(&ops);
    }
    midgen_dyndeinit(&ops);

    if (out.len != 1) {
        // handle operator and operand mismatch here
        printf("expr start at %d:%d\n", toks[start].pos.line,
               toks[start].pos.column);
        printf("expr end at %d:%d\n", toks[i].pos.line, toks[i].pos.column);
        printf("out len = %" MID_PRIisz "\n", out.len);
        MID_CRASH("mismatched operators and operands");
    }

    struct midpar_Expr ret = out.arr[0];
    midgen_dyndeinit(&out);
    midsema_typecheck_expr(&ret, scope, diags);
    return ret;
}

mid_isize midpar_skip_expr(const struct midlex_Token *toks, mid_isize start,
                           const enum midlex_TokenType *end_types,
                           mid_isize n_end_types, struct mid_DiagVec *diags)
{
    mid_isize i;
    for (i = start; !is_end_type(toks[i].type, end_types, n_end_types); ++i) {
        if (toks[i].type == MIDLEX_TOKENTYPE_L_PAREN) {
            mid_isize rparen = midpar_find_twin_paren(toks, i, MID_ISIZE_MAX);
            if (rparen == -1 && diags)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "')'", &toks[i], MIDDIAG_ERR_MISSING_PAREN));
            i = rparen == -1 ? i : rparen;
        } else if (toks[i].type == MIDLEX_TOKENTYPE_L_CURLY) {
            mid_isize rcurly = midpar_find_twin_curly(toks, i, MID_ISIZE_MAX);
            if (rcurly == -1 && diags)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "'}'", &toks[i], MIDDIAG_ERR_MISSING_CURLY));
            i = rcurly == -1 ? i : rcurly;
        } else if (toks[i].type == MIDLEX_TOKENTYPE_L_SQBRACKET) {
            mid_isize rsqbracket =
                midpar_find_twin_sqbracket(toks, i, MID_ISIZE_MAX);
            if (rsqbracket == -1 && diags)
                midgen_dynpush(
                    diags, middiag_expected_token_err(
                               "']'", &toks[i], MIDDIAG_ERR_MISSING_SQBRACKET));
            i = rsqbracket == -1 ? i : rsqbracket;
        } else if (toks[i].type == MIDLEX_TOKENTYPE_LT) {
            mid_isize rangle = midpar_find_twin_angle(toks, i, MID_ISIZE_MAX);
            if (rangle == -1 && diags)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "'>'", &toks[i], MIDDIAG_ERR_MISSING_ANGLE));
            i = rangle == -1 ? i : rangle;
        }
    }

    return i;
}

void midpar_Expr_deinit(struct midpar_Expr *expr)
{
    if (midpar_expr_uses_args(expr->type)) {
        for (mid_isize i = 0; i < expr->info.args.len; ++i)
            midpar_Expr_deinit(&expr->info.args.arr[i]);
        midgen_dyndeinit(&expr->info.args);
    } else if (midpar_is_fltlit(expr->type)) {
        mid_APFloat_deinit(&expr->info.val.flt);
    } else if (midpar_is_numlit(expr->type)) {
        mid_APInt_deinit(&expr->info.val.i);
    }

    midpar_Type_deinit(&expr->ret);
}

struct midpar_Expr midpar_copy_expr(const struct midpar_Expr *expr)
{
    struct midpar_Expr ret = *expr;

    ret.ret = midpar_copy_type(&expr->ret);

    if (midpar_expr_uses_args(expr->type)) {
        ret.info.args = (struct midpar_ExprVec){};
        midgen_dynreserve(&ret.info.args, expr->info.args.len);
        for (mid_isize i = 0; i < expr->info.args.len; ++i)
            midgen_dynpush(&ret.info.args,
                           midpar_copy_expr(&expr->info.args.arr[i]));
    }

    return ret;
}

bool midpar_expr_uses_args(enum midpar_ExprType type)
{
    return midpar_is_op(type);
}
