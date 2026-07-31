#include "expr.h"
#include "diag.h"
#include "find_twin.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "parser/end_types.h"
#include "parser/expr_type.h"
#include "parser/type.h"
#include "print.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <assert.h>
#include <stdio.h>

bool MidParser_is_strlit(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_STRING_LIT ||
           type == MIDPARSER_EXPRTYPE_WSTRING_LIT ||
           type == MIDPARSER_EXPRTYPE_STRING16_LIT ||
           type == MIDPARSER_EXPRTYPE_STRING32_LIT;
}

bool MidParser_is_numlit(enum MidParser_ExprType type)
{
    return type > MIDPARSER_EXPRTYPE_NUMMIDLIT_START &&
           type < MIDPARSER_EXPRTYPE_NUMMIDLIT_END;
}

bool MidParser_is_ternaryop(enum MidParser_ExprType type)
{
    return type > MIDPARSER_EXPRTYPE_TERNARYOP_START &&
           type < MIDPARSER_EXPRTYPE_TERNARYOP_END;
}

bool MidParser_is_binop(enum MidParser_ExprType type)
{
    return type > MIDPARSER_EXPRTYPE_BINOP_START &&
           type < MIDPARSER_EXPRTYPE_BINOP_END;
}

bool MidParser_is_unaryop(enum MidParser_ExprType type)
{
    return type > MIDPARSER_EXPRTYPE_UNARYOP_START &&
           type < MIDPARSER_EXPRTYPE_UNARYOP_END;
}

bool MidParser_is_scope_res(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_BIN_SCOPE_RES ||
           type == MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES;
}

bool MidParser_is_op(enum MidParser_ExprType type)
{
    return MidParser_is_binop(type) || MidParser_is_unaryop(type);
}

bool MidParser_is_arith_op(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_MUL || type == MIDPARSER_EXPRTYPE_DIV ||
           type == MIDPARSER_EXPRTYPE_MOD || type == MIDPARSER_EXPRTYPE_ADD ||
           type == MIDPARSER_EXPRTYPE_SUB ||
           type == MIDPARSER_EXPRTYPE_LEFT_SHIFT ||
           type == MIDPARSER_EXPRTYPE_RIGHT_SHIFT ||
           type == MIDPARSER_EXPRTYPE_BITWISE_AND ||
           type == MIDPARSER_EXPRTYPE_BITWISE_XOR ||
           type == MIDPARSER_EXPRTYPE_BITWISE_OR ||
           type == MIDPARSER_EXPRTYPE_BITWISE_NOT ||
           type == MIDPARSER_EXPRTYPE_UNARY_PLUS ||
           type == MIDPARSER_EXPRTYPE_UNARY_MINUS;
}

bool MidParser_is_logical_op(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_LOGICAL_AND ||
           type == MIDPARSER_EXPRTYPE_LOGICAL_OR ||
           type == MIDPARSER_EXPRTYPE_LOGICAL_NOT;
}

bool MidParser_is_comp_op(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_LT || type == MIDPARSER_EXPRTYPE_GT ||
           type == MIDPARSER_EXPRTYPE_LTEQ || type == MIDPARSER_EXPRTYPE_GTEQ ||
           type == MIDPARSER_EXPRTYPE_EQ || type == MIDPARSER_EXPRTYPE_NEQ;
}

bool MidParser_is_assignment(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_MUL_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_DIV_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_MOD_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_SUB_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_ADD_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_AND_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_OR_ASSIGN ||
           type == MIDPARSER_EXPRTYPE_XOR_ASSIGN;
}

bool MidParser_is_memb_sel(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_MEMB_SEL ||
           type == MIDPARSER_EXPRTYPE_PTR_MEMB_SEL ||
           type == MIDPARSER_EXPRTYPE_PTR_TO_MEMB_SEL ||
           type == MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
}

i32 MidParser_op_precedence(enum MidParser_ExprType op)
{
    // goes from 16 to 1
    i32 flipped;

    switch (op) {
    case MIDPARSER_EXPRTYPE_BIN_SCOPE_RES:
    case MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES:
        flipped = 1;
        break;

    case MIDPARSER_EXPRTYPE_MEMB_SEL:
    case MIDPARSER_EXPRTYPE_PTR_MEMB_SEL:
    case MIDPARSER_EXPRTYPE_ARRAY_SUBSCR:
    case MIDPARSER_EXPRTYPE_FUNC_CALL:
    case MIDPARSER_EXPRTYPE_POSTFIX_INC:
    case MIDPARSER_EXPRTYPE_POSTFIX_DEC:
    case MIDPARSER_EXPRTYPE_TYPEID:
    case MIDPARSER_EXPRTYPE_CONSTCAST:
    case MIDPARSER_EXPRTYPE_DYNAMICCAST:
    case MIDPARSER_EXPRTYPE_REINTERPRETCAST:
    case MIDPARSER_EXPRTYPE_STATICCAST:
        flipped = 2;
        break;

    case MIDPARSER_EXPRTYPE_SIZEOF:
    case MIDPARSER_EXPRTYPE_PREFIX_INC:
    case MIDPARSER_EXPRTYPE_PREFIX_DEC:
    case MIDPARSER_EXPRTYPE_BITWISE_NOT:
    case MIDPARSER_EXPRTYPE_LOGICAL_NOT:
    case MIDPARSER_EXPRTYPE_UNARY_MINUS:
    case MIDPARSER_EXPRTYPE_UNARY_PLUS:
    case MIDPARSER_EXPRTYPE_REF:
    case MIDPARSER_EXPRTYPE_DEREF:
    case MIDPARSER_EXPRTYPE_NEW:
    case MIDPARSER_EXPRTYPE_DELETE:
    case MIDPARSER_EXPRTYPE_CAST:
        flipped = 3;
        break;

    case MIDPARSER_EXPRTYPE_PTR_TO_MEMB_SEL:
    case MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        flipped = 4;
        break;

    case MIDPARSER_EXPRTYPE_MUL:
    case MIDPARSER_EXPRTYPE_DIV:
    case MIDPARSER_EXPRTYPE_MOD:
        flipped = 5;
        break;

    case MIDPARSER_EXPRTYPE_ADD:
    case MIDPARSER_EXPRTYPE_SUB:
        flipped = 6;
        break;

    case MIDPARSER_EXPRTYPE_LEFT_SHIFT:
    case MIDPARSER_EXPRTYPE_RIGHT_SHIFT:
        flipped = 7;
        break;

    case MIDPARSER_EXPRTYPE_LT:
    case MIDPARSER_EXPRTYPE_GT:
    case MIDPARSER_EXPRTYPE_LTEQ:
    case MIDPARSER_EXPRTYPE_GTEQ:
        flipped = 8;
        break;

    case MIDPARSER_EXPRTYPE_EQ:
    case MIDPARSER_EXPRTYPE_NEQ:
        flipped = 9;
        break;

    case MIDPARSER_EXPRTYPE_BITWISE_AND:
        flipped = 10;
        break;

    case MIDPARSER_EXPRTYPE_BITWISE_XOR:
        flipped = 11;
        break;

    case MIDPARSER_EXPRTYPE_BITWISE_OR:
        flipped = 12;
        break;

    case MIDPARSER_EXPRTYPE_LOGICAL_AND:
        flipped = 13;
        break;

    case MIDPARSER_EXPRTYPE_LOGICAL_OR:
        flipped = 14;
        break;

    case MIDPARSER_EXPRTYPE_CONDITIONAL:
    case MIDPARSER_EXPRTYPE_ASSIGN:
    case MIDPARSER_EXPRTYPE_MUL_ASSIGN:
    case MIDPARSER_EXPRTYPE_DIV_ASSIGN:
    case MIDPARSER_EXPRTYPE_MOD_ASSIGN:
    case MIDPARSER_EXPRTYPE_ADD_ASSIGN:
    case MIDPARSER_EXPRTYPE_SUB_ASSIGN:
    case MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
    case MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
    case MIDPARSER_EXPRTYPE_AND_ASSIGN:
    case MIDPARSER_EXPRTYPE_XOR_ASSIGN:
    case MIDPARSER_EXPRTYPE_OR_ASSIGN:
    case MIDPARSER_EXPRTYPE_THROW:
        flipped = 15;
        break;

    case MIDPARSER_EXPRTYPE_COMMA:
        flipped = 16;
        break;

    default:
        MID_CRASH("expr isn't an operator");
    }

    return 16 - flipped;
}

bool MidParser_op_ltr_assoc(enum MidParser_ExprType op)
{
    i32 prec = MidParser_op_precedence(op);
    return prec != 15 && prec != 13 && prec != 1;
}

bool MidParser_is_glvalue(enum MidParser_ExprValueType type)
{
    return type == MIDPARSER_EXPRVALUE_LVALUE ||
           type == MIDPARSER_EXPRVALUE_XVALUE;
}

bool MidParser_is_rvalue(enum MidParser_ExprValueType type)
{
    return type == MIDPARSER_EXPRVALUE_PRVALUE ||
           type == MIDPARSER_EXPRVALUE_XVALUE;
}

bool MidParser_is_rvalue(enum MidParser_ExprValueType type);

static struct MidParser_Expr lit_tok_to_expr(const struct MidLexer_Token *tok)
{
    assert(MidLexer_is_lit(tok->type));

    struct MidParser_Expr ret = {.tok = tok,
                                 .info.val = tok->val,
                                 .valtype = MIDPARSER_EXPRVALUE_PRVALUE};

    switch (tok->type) {
    case MIDLEXER_TOKENTYPE_CHAR_LIT:
        ret.type = MIDPARSER_EXPRTYPE_CHAR_LIT;
        break;

    case MIDLEXER_TOKENTYPE_WCHAR_LIT:
        ret.type = MIDPARSER_EXPRTYPE_WCHAR_LIT;
        break;

    case MIDLEXER_TOKENTYPE_CHAR16_LIT:
        ret.type = MIDPARSER_EXPRTYPE_CHAR16_LIT;
        break;

    case MIDLEXER_TOKENTYPE_CHAR32_LIT:
        ret.type = MIDPARSER_EXPRTYPE_CHAR32_LIT;
        break;

    case MIDLEXER_TOKENTYPE_STRING_LIT:
        ret.type = MIDPARSER_EXPRTYPE_STRING_LIT;
        break;

    case MIDLEXER_TOKENTYPE_WSTRING_LIT:
        ret.type = MIDPARSER_EXPRTYPE_WSTRING_LIT;
        break;

    case MIDLEXER_TOKENTYPE_STRING16_LIT:
        ret.type = MIDPARSER_EXPRTYPE_STRING16_LIT;
        break;

    case MIDLEXER_TOKENTYPE_STRING32_LIT:
        ret.type = MIDPARSER_EXPRTYPE_STRING32_LIT;
        break;

    case MIDLEXER_TOKENTYPE_INT_LIT:
        ret.type = MIDPARSER_EXPRTYPE_INT_LIT;
        break;

    case MIDLEXER_TOKENTYPE_UINT_LIT:
        ret.type = MIDPARSER_EXPRTYPE_UINT_LIT;
        break;

    case MIDLEXER_TOKENTYPE_LONG_LIT:
        ret.type = MIDPARSER_EXPRTYPE_LONG_LIT;
        break;

    case MIDLEXER_TOKENTYPE_ULONG_LIT:
        ret.type = MIDPARSER_EXPRTYPE_ULONG_LIT;
        break;

    case MIDLEXER_TOKENTYPE_LONGLONG_LIT:
        ret.type = MIDPARSER_EXPRTYPE_LONGLONG_LIT;
        break;

    case MIDLEXER_TOKENTYPE_ULONGLONG_LIT:
        ret.type = MIDPARSER_EXPRTYPE_ULONGLONG_LIT;
        break;

    case MIDLEXER_TOKENTYPE_FLOAT_LIT:
        ret.type = MIDPARSER_EXPRTYPE_FLOAT_LIT;
        break;

    case MIDLEXER_TOKENTYPE_DOUBLE_LIT:
        ret.type = MIDPARSER_EXPRTYPE_DOUBLE_LIT;
        break;

    case MIDLEXER_TOKENTYPE_LONGDOUBLE_LIT:
        ret.type = MIDPARSER_EXPRTYPE_LONGDOUBLE_LIT;
        break;

    case MIDLEXER_TOKENTYPE_BOOL_LIT:
        ret.type = MIDPARSER_EXPRTYPE_BOOL_LIT;
        break;

    case MIDLEXER_TOKENTYPE_NULLPTR_LIT:
        ret.type = MIDPARSER_EXPRTYPE_NULLPTR_LIT;
        break;

    default:
        MID_CRASH("token is not literal");
    }

    return ret;
}

static struct MidParser_Expr ident_tok_to_expr(const struct MidLexer_Token *tok)
{
    assert(tok->type == MIDLEXER_TOKENTYPE_IDENTIFIER);

    struct MidParser_Expr ret = {.tok = tok,
                                 .info.ident = tok->ident,
                                 .type = MIDPARSER_EXPRTYPE_IDENTIFIER};
    return ret;
}

static struct MidParser_Expr this_tok_to_expr(const struct MidLexer_Token *tok)
{
    struct MidParser_Expr ret = {
        .tok = tok, .info.ident = tok->ident, .type = MIDPARSER_EXPRTYPE_THIS};
    return ret;
}

static struct MidParser_Expr
op_tok_to_expr_mode0(const struct MidLexer_Token *tok,
                     struct MidDiag_DiagVec *diags)
{
    struct MidParser_Expr ret = {.tok = tok};

    switch (tok->type) {
    case MIDLEXER_TOKENTYPE_SCOPE_RES:
        ret.type = MIDPARSER_EXPRTYPE_BIN_SCOPE_RES;
        break;

    case MIDLEXER_TOKENTYPE_MEMB_SEL:
        ret.type = MIDPARSER_EXPRTYPE_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_PTR_MEMB_SEL:
        ret.type = MIDPARSER_EXPRTYPE_PTR_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_L_SQBRACKET:
        ret.type = MIDPARSER_EXPRTYPE_ARRAY_SUBSCR;
        break;

    case MIDLEXER_TOKENTYPE_L_PAREN:
        ret.type = MIDPARSER_EXPRTYPE_FUNC_CALL;
        break;

    case MIDLEXER_TOKENTYPE_INC:
        ret.type = MIDPARSER_EXPRTYPE_POSTFIX_INC;
        break;

    case MIDLEXER_TOKENTYPE_DEC:
        ret.type = MIDPARSER_EXPRTYPE_POSTFIX_DEC;
        break;

    case MIDLEXER_TOKENTYPE_TYPEID:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "typeid", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_TYPEID;
        break;

    case MIDLEXER_TOKENTYPE_CONSTCAST:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("const_cast", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_CONSTCAST;
        break;

    case MIDLEXER_TOKENTYPE_DYNAMICCAST:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("dynamic_cast", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_DYNAMICCAST;
        break;

    case MIDLEXER_TOKENTYPE_REINTERPRETCAST:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("reinterpret_cast", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_REINTERPRETCAST;
        break;

    case MIDLEXER_TOKENTYPE_STATICCAST:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("static_cast", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_STATICCAST;
        break;

    case MIDLEXER_TOKENTYPE_SIZEOF:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "sizeof", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_SIZEOF;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_NOT:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("bitwise NOT '~'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_NOT;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_NOT:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("logical NOT '!'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LOGICAL_NOT;
        break;

    case MIDLEXER_TOKENTYPE_SUB:
        ret.type = MIDPARSER_EXPRTYPE_SUB;
        break;

    case MIDLEXER_TOKENTYPE_ADD:
        ret.type = MIDPARSER_EXPRTYPE_ADD;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_AND:
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_AND;
        break;

    case MIDLEXER_TOKENTYPE_MUL:
        ret.type = MIDPARSER_EXPRTYPE_MUL;
        break;

    case MIDLEXER_TOKENTYPE_NEW:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "new", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_NEW;
        break;

    case MIDLEXER_TOKENTYPE_DELETE:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "delete", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_DELETE;
        break;

    case MIDLEXER_TOKENTYPE_PTR_TO_MEMB_SEL:
        ret.type = MIDPARSER_EXPRTYPE_PTR_TO_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        ret.type = MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_DIV:
        ret.type = MIDPARSER_EXPRTYPE_DIV;
        break;

    case MIDLEXER_TOKENTYPE_MOD:
        ret.type = MIDPARSER_EXPRTYPE_MOD;
        break;

    case MIDLEXER_TOKENTYPE_LEFT_SHIFT:
        ret.type = MIDPARSER_EXPRTYPE_LEFT_SHIFT;
        break;

    case MIDLEXER_TOKENTYPE_RIGHT_SHIFT:
        ret.type = MIDPARSER_EXPRTYPE_RIGHT_SHIFT;
        break;

    case MIDLEXER_TOKENTYPE_LT:
        ret.type = MIDPARSER_EXPRTYPE_LT;
        break;

    case MIDLEXER_TOKENTYPE_GT:
        ret.type = MIDPARSER_EXPRTYPE_GT;
        break;

    case MIDLEXER_TOKENTYPE_LTEQ:
        ret.type = MIDPARSER_EXPRTYPE_LTEQ;
        break;

    case MIDLEXER_TOKENTYPE_GTEQ:
        ret.type = MIDPARSER_EXPRTYPE_GTEQ;
        break;

    case MIDLEXER_TOKENTYPE_EQ:
        ret.type = MIDPARSER_EXPRTYPE_EQ;
        break;

    case MIDLEXER_TOKENTYPE_NEQ:
        ret.type = MIDPARSER_EXPRTYPE_NEQ;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_XOR:
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_XOR;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_OR:
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_OR;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_AND:
        ret.type = MIDPARSER_EXPRTYPE_LOGICAL_AND;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_OR:
        ret.type = MIDPARSER_EXPRTYPE_LOGICAL_OR;
        break;

    case MIDLEXER_TOKENTYPE_CONDITIONAL:
        ret.type = MIDPARSER_EXPRTYPE_CONDITIONAL;
        break;

    case MIDLEXER_TOKENTYPE_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_MUL_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_MUL_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_DIV_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_DIV_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_MOD_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_MOD_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_ADD_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_ADD_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_SUB_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_SUB_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_AND_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_AND_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_OR_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_OR_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_XOR_ASSIGN:
        ret.type = MIDPARSER_EXPRTYPE_XOR_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_THROW:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "throw", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_THROW;
        break;

    case MIDLEXER_TOKENTYPE_COMMA:
        ret.type = MIDPARSER_EXPRTYPE_COMMA;
        break;

    default:
        MID_CRASH("can't convert token to expr");
    }

    return ret;
}

static struct MidParser_Expr
op_tok_to_expr_mode1(const struct MidLexer_Token *tok,
                     struct MidDiag_DiagVec *diags)
{
    struct MidParser_Expr ret = {.tok = tok};

    switch (tok->type) {
    case MIDLEXER_TOKENTYPE_SCOPE_RES:
        ret.type = MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES;
        break;

    case MIDLEXER_TOKENTYPE_MEMB_SEL:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("member select '.'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_PTR_MEMB_SEL:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "ptr to member select '->'", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_PTR_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_L_SQBRACKET:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("array subscript '[]'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_ARRAY_SUBSCR;
        break;

    case MIDLEXER_TOKENTYPE_L_PAREN:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("function call '()'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_FUNC_CALL;
        break;

    case MIDLEXER_TOKENTYPE_INC:
        ret.type = MIDPARSER_EXPRTYPE_PREFIX_INC;
        break;

    case MIDLEXER_TOKENTYPE_DEC:
        ret.type = MIDPARSER_EXPRTYPE_PREFIX_DEC;
        break;

    case MIDLEXER_TOKENTYPE_TYPEID:
        ret.type = MIDPARSER_EXPRTYPE_TYPEID;
        break;

    case MIDLEXER_TOKENTYPE_CONSTCAST:
        ret.type = MIDPARSER_EXPRTYPE_CONSTCAST;
        break;

    case MIDLEXER_TOKENTYPE_DYNAMICCAST:
        ret.type = MIDPARSER_EXPRTYPE_DYNAMICCAST;
        break;

    case MIDLEXER_TOKENTYPE_REINTERPRETCAST:
        ret.type = MIDPARSER_EXPRTYPE_REINTERPRETCAST;
        break;

    case MIDLEXER_TOKENTYPE_STATICCAST:
        ret.type = MIDPARSER_EXPRTYPE_STATICCAST;
        break;

    case MIDLEXER_TOKENTYPE_SIZEOF:
        ret.type = MIDPARSER_EXPRTYPE_SIZEOF;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_NOT:
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_NOT;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_NOT:
        ret.type = MIDPARSER_EXPRTYPE_LOGICAL_NOT;
        break;

    case MIDLEXER_TOKENTYPE_SUB:
        ret.type = MIDPARSER_EXPRTYPE_UNARY_MINUS;
        break;

    case MIDLEXER_TOKENTYPE_ADD:
        ret.type = MIDPARSER_EXPRTYPE_UNARY_PLUS;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_AND:
        ret.type = MIDPARSER_EXPRTYPE_REF;
        break;

    case MIDLEXER_TOKENTYPE_MUL:
        ret.type = MIDPARSER_EXPRTYPE_DEREF;
        break;

    case MIDLEXER_TOKENTYPE_NEW:
        ret.type = MIDPARSER_EXPRTYPE_NEW;
        break;

    case MIDLEXER_TOKENTYPE_DELETE:
        ret.type = MIDPARSER_EXPRTYPE_DELETE;
        break;

    case MIDLEXER_TOKENTYPE_PTR_TO_MEMB_SEL:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "ptr to member select '.*'", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_PTR_TO_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "ptr to ptr member select '->*'", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
        break;

    case MIDLEXER_TOKENTYPE_DIV:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("division '/'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_DIV;
        break;

    case MIDLEXER_TOKENTYPE_MOD:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("modulo '%'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_MOD;
        break;

    case MIDLEXER_TOKENTYPE_LEFT_SHIFT:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("left shift '<<'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LEFT_SHIFT;
        break;

    case MIDLEXER_TOKENTYPE_RIGHT_SHIFT:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("right shift '>>'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_RIGHT_SHIFT;
        break;

    case MIDLEXER_TOKENTYPE_LT:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("less than '<'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LT;
        break;

    case MIDLEXER_TOKENTYPE_GT:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("greater than '>'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_GT;
        break;

    case MIDLEXER_TOKENTYPE_LTEQ:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("less than or equal '<='", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LTEQ;
        break;

    case MIDLEXER_TOKENTYPE_GTEQ:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "greater than or equal '>='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_GTEQ;
        break;

    case MIDLEXER_TOKENTYPE_EQ:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("equality '=='", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_EQ;
        break;

    case MIDLEXER_TOKENTYPE_NEQ:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("inequality '!='", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_NEQ;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_XOR:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("bitwise XOR '^'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_XOR;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_OR:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("bitwise OR '|'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_BITWISE_OR;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_AND:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("logical AND '&&'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LOGICAL_AND;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_OR:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("logical OR '||'", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LOGICAL_OR;
        break;

    case MIDLEXER_TOKENTYPE_CONDITIONAL:
        MidGen_dynpush(
            diags, MidDiag_unexpected_token_err("conditional operator '?'", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_CONDITIONAL;
        break;

    case MIDLEXER_TOKENTYPE_ASSIGN:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("assignment '=='", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_MUL_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "multiplication assignment '*='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_MUL_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_DIV_ASSIGN:
        MidGen_dynpush(
            diags, MidDiag_unexpected_token_err("division assignment '/='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_DIV_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_MOD_ASSIGN:
        MidGen_dynpush(diags,
                    MidDiag_unexpected_token_err("modulus assignment '%='", tok,
                                                 MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_MOD_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_ADD_ASSIGN:
        MidGen_dynpush(
            diags, MidDiag_unexpected_token_err("addition assignment '+='", tok,
                                                MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_ADD_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_SUB_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "subtraction assignment '-='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_SUB_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "left shift assignment '<<='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "right shift assignment '>>='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_AND_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "bitwise AND assignment '&='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_AND_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_OR_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "bitwise OR assignment '|='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_OR_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_XOR_ASSIGN:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "bitwise XOR assignment '^='", tok,
                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_XOR_ASSIGN;
        break;

    case MIDLEXER_TOKENTYPE_THROW:
        ret.type = MIDPARSER_EXPRTYPE_THROW;
        break;

    case MIDLEXER_TOKENTYPE_COMMA:
        MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                               "comma ','", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        ret.type = MIDPARSER_EXPRTYPE_COMMA;
        break;

    default:
        MID_CRASH("can't convert token to expr");
    }

    return ret;
}

static void parse_func_call_args(struct MidParser_Expr *f_call,
                                 const struct MidLexer_Token *toks,
                                 mid_isize lparen, mid_isize *out_rparen,
                                 struct MidSema_Scope *scope,
                                 struct MidDiag_DiagVec *diags)
{
    mid_isize rparen = MidParser_find_twin_paren(toks, lparen, MID_ISIZE_MAX);
    if (rparen == -1) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("')'", &toks[lparen],
                                               MIDDIAG_ERR_MISSING_PAREN));
        rparen = lparen;
    }
    if (out_rparen)
        *out_rparen = rparen;

    for (mid_isize i = lparen + 1; i < rparen; ++i) {
        auto arg = MidParser_parse_expr(toks, i, MIDPARSER_ARG_ENDTYPES, &i,
                                        scope, diags);
        MidGen_dynpush(&f_call->info.args, arg);

        if (toks[i].type != MIDLEXER_TOKENTYPE_R_PAREN &&
            toks[i].type != MIDLEXER_TOKENTYPE_COMMA) {
            MidGen_dynpush(diags,
                        MidDiag_expected_token_err("','", &toks[lparen],
                                                   MIDDIAG_ERR_MISSING_PAREN));
        }
    }
}

// in most cases out_end_idx is set to idx, but in some cases like func calls
// and array subscripts out_end_idx is set to the last token in the expr:
// func(a, b, c, d)
//     ^          ^
//    idx    out_end_idx
static struct MidParser_Expr op_tok_to_expr(const struct MidLexer_Token *toks,
                                            mid_isize idx, mid_isize *out_end_idx,
                                            bool mode,
                                            struct MidSema_Scope *scope,
                                            struct MidDiag_DiagVec *diags)
{
    struct MidParser_Expr ret = mode ? op_tok_to_expr_mode1(&toks[idx], diags)
                                     : op_tok_to_expr_mode0(&toks[idx], diags);

    if (ret.type == MIDPARSER_EXPRTYPE_FUNC_CALL)
        parse_func_call_args(&ret, toks, idx, out_end_idx, scope, diags);
    else if (out_end_idx)
        *out_end_idx = idx;

    return ret;
}

static bool has_enough_operands(enum MidParser_ExprType op, int n)
{
    if (MidParser_is_unaryop(op))
        return n >= 1;
    else if (MidParser_is_binop(op))
        return n >= 2;
    else if (MidParser_is_ternaryop(op))
        return n >= 3;
    else
        MID_CRASH("bad expression type");
}

static void add_op_to_out(struct MidParser_Expr *op,
                          struct MidParser_ExprVec *out,
                          struct MidDiag_DiagVec *diags)
{
    if (!has_enough_operands(op->type, out->len)) {
        struct MidDiag_Diag err = {
            .pos = op->tok->pos,
            .line = op->tok->line,
            .msg = MidPrint_fmt_to_str(
                "%s operator expects %d %s, received %" PRIisz,
                MidParser_is_unaryop(op->type) ? "unary"
                : MidParser_is_binop(op->type) ? "binary"
                                               : "ternary",
                MidParser_is_unaryop(op->type) ? 1
                : MidParser_is_binop(op->type) ? 2
                                               : 3,
                MidParser_is_unaryop(op->type) ? "operand" : "operands",
                out->len),
            .err = MIDDIAG_ERR_INSUFFICIENT_OPERANDS,
            .type = MIDDIAG_TYPE_ERROR};
        MidGen_dynpush(diags, err);
        return;
    }

    // the exprs at the top act as operands for the new op
    if (MidParser_is_ternaryop(op->type))
        MidGen_dynpush(&op->info.args, out->arr[out->len - 3]);
    if (MidParser_is_ternaryop(op->type) || MidParser_is_binop(op->type))
        MidGen_dynpush(&op->info.args, out->arr[out->len - 2]);
    if (op->type == MIDPARSER_EXPRTYPE_FUNC_CALL && op->info.args.len > 0)
        // func calls already have the arguments pushed into args, so we gotta
        // use insert to put the identifier being called first
        MidGen_dyninsert(&op->info.args, 0, out->arr[out->len - 1]);
    else
        MidGen_dynpush(&op->info.args, out->arr[out->len - 1]);

    // the expressions are now encoded in op
    if (MidParser_is_ternaryop(op->type))
        MidGen_dynpop(out);
    if (MidParser_is_ternaryop(op->type) || MidParser_is_binop(op->type))
        MidGen_dynpop(out);
    MidGen_dynpop(out);

    MidGen_dynpush(out, *op);
}

// handles sending an operator through the shunting yard
static void push_operator(const struct MidLexer_Token *toks, mid_isize idx,
                          mid_isize *out_end_idx, struct MidParser_ExprVec *out,
                          struct MidParser_ExprVec *ops, bool mode,
                          struct MidSema_Scope *scope,
                          struct MidDiag_DiagVec *diags)
{
    struct MidParser_Expr op =
        op_tok_to_expr(toks, idx, out_end_idx, mode, scope, diags);

    // remove any greater precedence operators
    struct MidParser_Expr *top = &ops->arr[ops->len - 1];
    while (ops->len > 0) {
        i32 op_prec = MidParser_op_precedence(op.type);
        i32 top_prec = MidParser_op_precedence(top->type);

        if (top_prec > op_prec ||
            (top_prec == op_prec && MidParser_op_ltr_assoc(op.type))) {
            add_op_to_out(top, out, diags);
            MidGen_dynpop(ops);
            top = &ops->arr[ops->len - 1];
        } else {
            break;
        }
    }

    MidGen_dynpush(ops, op);
}

// a sub expression is a part of an expression encased in parentheses
static struct MidParser_Expr parse_subexpr(const struct MidLexer_Token *toks,
                                           mid_isize l_paren, mid_isize *out_end,
                                           struct MidSema_Scope *scope,
                                           struct MidDiag_DiagVec *diags)
{
    if (MidParser_find_twin_paren(toks, l_paren, MID_ISIZE_MAX) == -1) {
        struct MidDiag_Diag err = {.pos = toks[l_paren].pos,
                                   .line = toks[l_paren].line,
                                   .msg = MidPrint_fmt_to_str("expected ')'"),
                                   .err = MIDDIAG_ERR_MISSING_PAREN,
                                   .type = MIDDIAG_TYPE_ERROR};
        MidGen_dynpush(diags, err);
    }

    return MidParser_parse_expr(
        toks, l_paren + 1,
        (enum MidLexer_TokenType[]){MIDLEXER_TOKENTYPE_R_PAREN}, 1, out_end,
        scope, diags);
}

static bool is_end_type(enum MidLexer_TokenType type,
                        const enum MidLexer_TokenType *end_types,
                        mid_isize n_end_types)
{
    for (mid_isize i = 0; i < n_end_types; ++i)
        if (type == end_types[i])
            return true;
    return false;
}

struct MidParser_Expr
MidParser_parse_expr(const struct MidLexer_Token *toks, mid_isize start,
                     const enum MidLexer_TokenType *end_types,
                     mid_isize n_end_types, mid_isize *out_end,
                     struct MidSema_Scope *scope, struct MidDiag_DiagVec *diags)
{
    // uses the shunting yard algorithm

    struct MidParser_ExprVec out = MidGen_dyninit();
    struct MidParser_ExprVec ops = MidGen_dyninit();

    // when false, binary operators remain binary and unary operators are
    // treated as postifx operators
    // when true, binary operators are treated as unary and unary operators are
    // treated as prefix operators
    // becomes true after finding an operand, becomes false after finding an
    // operator unless it's a unary postfix operator
    bool mode = true;

    mid_isize i;
    for (i = start; !is_end_type(toks[i].type, end_types, n_end_types); ++i) {
        if (MidLexer_is_lit(toks[i].type)) {
            if (!mode)
                MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                                       "literal", &toks[i],
                                       MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                MidGen_dynpush(&out, lit_tok_to_expr(&toks[i]));
            mode = false;
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_IDENTIFIER) {
            if (!mode)
                MidGen_dynpush(diags, MidDiag_unexpected_token_err(
                                       "identifier", &toks[i],
                                       MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                MidGen_dynpush(&out, ident_tok_to_expr(&toks[i]));
            mode = false;
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_THIS) {
            if (!mode)
                MidGen_dynpush(
                    diags, MidDiag_unexpected_token_err(
                               "this", &toks[i], MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                MidGen_dynpush(&out, this_tok_to_expr(&toks[i]));
            mode = false;
        } else if (MidLexer_is_op(toks[i].type)) {
            push_operator(toks, i, &i, &out, &ops, mode, scope, diags);
            mode =
                ops.arr[ops.len - 1].type != MIDPARSER_EXPRTYPE_POSTFIX_DEC &&
                ops.arr[ops.len - 1].type != MIDPARSER_EXPRTYPE_POSTFIX_INC;
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_L_PAREN) {
            if (!mode)
                // if mode is 0, a sub-expression is actually a function call
                push_operator(toks, i, &i, &out, &ops, mode, scope, diags);
            else
                MidGen_dynpush(&out, parse_subexpr(toks, i, &i, scope, diags));
            mode = false;
        }
    }
    if (out_end)
        *out_end = i;

    // excess operators just get popped in fifo order
    while (ops.len > 0) {
        add_op_to_out(&ops.arr[ops.len - 1], &out, diags);
        MidGen_dynpop(&ops);
    }
    MidGen_dyndeinit(&ops);

    if (out.len != 1) {
        // handle operator and operand mismatch here
        printf("expr start at %d:%d\n", toks[start].pos.line,
               toks[start].pos.column);
        printf("expr end at %d:%d\n", toks[i].pos.line, toks[i].pos.column);
        printf("out len = %" PRIisz "\n", out.len);
        MID_CRASH("mismatched operators and operands");
    }

    struct MidParser_Expr ret = out.arr[0];
    MidGen_dyndeinit(&out);
    MidSema_typecheck_expr(&ret, scope, diags);
    return ret;
}

mid_isize MidParser_skip_expr(const struct MidLexer_Token *toks, mid_isize start,
                            const enum MidLexer_TokenType *end_types,
                            mid_isize n_end_types, struct MidDiag_DiagVec *diags)
{
    mid_isize i;
    for (i = start; !is_end_type(toks[i].type, end_types, n_end_types); ++i) {
        if (toks[i].type == MIDLEXER_TOKENTYPE_L_PAREN) {
            mid_isize rparen = MidParser_find_twin_paren(toks, i, MID_ISIZE_MAX);
            if (rparen == -1 && diags)
                MidGen_dynpush(diags,
                            MidDiag_expected_token_err(
                                "')'", &toks[i], MIDDIAG_ERR_MISSING_PAREN));
            i = rparen == -1 ? i : rparen;
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_L_CURLY) {
            mid_isize rcurly = MidParser_find_twin_curly(toks, i, MID_ISIZE_MAX);
            if (rcurly == -1 && diags)
                MidGen_dynpush(diags,
                            MidDiag_expected_token_err(
                                "'}'", &toks[i], MIDDIAG_ERR_MISSING_CURLY));
            i = rcurly == -1 ? i : rcurly;
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_L_SQBRACKET) {
            mid_isize rsqbracket =
                MidParser_find_twin_sqbracket(toks, i, MID_ISIZE_MAX);
            if (rsqbracket == -1 && diags)
                MidGen_dynpush(
                    diags, MidDiag_expected_token_err(
                               "']'", &toks[i], MIDDIAG_ERR_MISSING_SQBRACKET));
            i = rsqbracket == -1 ? i : rsqbracket;
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_LT) {
            mid_isize rangle = MidParser_find_twin_angle(toks, i, MID_ISIZE_MAX);
            if (rangle == -1 && diags)
                MidGen_dynpush(diags,
                            MidDiag_expected_token_err(
                                "'>'", &toks[i], MIDDIAG_ERR_MISSING_ANGLE));
            i = rangle == -1 ? i : rangle;
        }
    }

    return i;
}

void MidParser_Expr_deinit(struct MidParser_Expr *expr)
{
    if (MidParser_expr_uses_args(expr->type)) {
        for (mid_isize i = 0; i < expr->info.args.len; ++i)
            MidParser_Expr_deinit(&expr->info.args.arr[i]);
        MidGen_dyndeinit(&expr->info.args);
    }

    MidParser_Type_deinit(&expr->ret);
}

struct MidParser_Expr MidParser_copy_expr(const struct MidParser_Expr *expr)
{
    struct MidParser_Expr ret = *expr;

    ret.ret = MidParser_copy_type(&expr->ret);

    if (MidParser_expr_uses_args(expr->type)) {
        ret.info.args = (struct MidParser_ExprVec){};
        MidGen_dynreserve(&ret.info.args, expr->info.args.len);
        for (mid_isize i = 0; i < expr->info.args.len; ++i)
            MidGen_dynpush(&ret.info.args,
                        MidParser_copy_expr(&expr->info.args.arr[i]));
    }

    return ret;
}

bool MidParser_expr_uses_args(enum MidParser_ExprType type)
{
    return MidParser_is_op(type);
}
