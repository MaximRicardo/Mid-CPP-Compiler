#include "parser/expr.h"
#include "cmd.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "literal.h"
#include "macros.h"
#include "parser/end_types.h"
#include "parser/expr_type.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "sema/expr.h"
#include "sema/scope.h"
#include "sema/typecheck.h"
#include <assert.h>
#include <stdio.h>

bool midpar_is_rvalue(enum midpar_ExprValueType type);

static struct midpar_Expr lit_tok_to_expr(const struct midlex_Token *tok)
{
    assert(midlex_is_lit(tok->type));

    struct midpar_Expr ret = {.tok = tok,
                              .info.val.v = tok->val,
                              .valtype = MIDPAR_EXPRVALUE_PRVALUE};

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

    ret.info.val.kind = midsema_lit_expr_value_kind(ret.type);

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
                                 midlex_TokenIter lparen,
                                 midlex_TokenIter *out_rparen,
                                 struct midsema_Scope *scope,
                                 struct mid_DiagVec *diags)
{
    auto rparen = midpar_find_twin_paren(lparen, nullptr);
    if (rparen == nullptr) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "')'", lparen, MIDDIAG_ERR_MISSING_PAREN));
        rparen = lparen;
    }
    if (out_rparen)
        *out_rparen = rparen;

    for (auto i = lparen + 1; i < rparen; ++i) {
        auto arg = midpar_parse_expr(i, MIDPAR_ARG_ENDTYPES, &i, scope, diags);
        midgen_dynpush(&f_call->info.args, arg);

        if (i->type != MIDLEX_TOKENTYPE_R_PAREN &&
            i->type != MIDLEX_TOKENTYPE_COMMA) {
            midgen_dynpush(
                diags, middiag_expected_token_err("','", lparen,
                                                  MIDDIAG_ERR_MISSING_PAREN));
        }
    }
}

static void parse_arr_subscr(struct midpar_Expr *subscr,
                             midlex_TokenIter l_bracket,
                             midlex_TokenIter *out_r_bracket,
                             struct midsema_Scope *scope,
                             struct mid_DiagVec *diags)
{
    midlex_TokenIter r_bracket = midpar_find_twin_sqbracket(l_bracket, nullptr);
    if (!r_bracket) {
        midgen_dynpush(
            diags, middiag_expected_token_err("']'", l_bracket,
                                              MIDDIAG_ERR_MISSING_SQBRACKET));
        r_bracket = l_bracket;
    }
    if (out_r_bracket)
        *out_r_bracket = r_bracket;

    struct midpar_Expr idx = midpar_parse_expr(
        l_bracket + 1, MIDPAR_SUBSCRIPT_ENDTYPES, nullptr, scope, diags);
    midgen_dynpush(&subscr->info.args, idx);
}

// in most cases out_end_idx is set to idx, but in some cases like func calls
// and array subscripts out_end_idx is set to the last token in the expr:
// func(a, b, c, d)
//     ^          ^
//    idx    out_end_idx
static struct midpar_Expr op_tok_to_expr(midlex_TokenIter tok,
                                         midlex_TokenIter *out_end, bool mode,
                                         struct midsema_Scope *scope,
                                         struct mid_DiagVec *diags)
{
    struct midpar_Expr ret = mode ? op_tok_to_expr_mode1(tok, diags)
                                  : op_tok_to_expr_mode0(tok, diags);

    if (ret.type == MIDPAR_EXPRTYPE_FUNC_CALL)
        parse_func_call_args(&ret, tok, out_end, scope, diags);
    else if (ret.type == MIDPAR_EXPRTYPE_ARRAY_SUBSCR)
        parse_arr_subscr(&ret, tok, out_end, scope, diags);
    else if (out_end)
        *out_end = tok;

    return ret;
}

static bool has_enough_operands(enum midpar_ExprType op, int n)
{
    if (midsema_is_unaryop(op) || op == MIDPAR_EXPRTYPE_ARRAY_SUBSCR)
        return n >= 1;
    else if (midsema_is_binop(op))
        return n >= 2;
    else if (midsema_is_ternaryop(op))
        return n >= 3;
    else
        MID_CRASH("bad expression type");
}

static struct mid_Diag not_enough_operands_err(const struct midpar_Expr *op,
                                               const struct midpar_ExprVec *out)
{
    return (struct mid_Diag){
        .pos = op->tok->pos,
        .line = op->tok->line,
        .msg = midcmd_fmt_to_str(
            "%s operator expects %d %s, received %" MID_PRIisz,
            midsema_is_unaryop(op->type) ? "unary"
            : midsema_is_binop(op->type) ? "binary"
                                         : "ternary",
            midsema_is_unaryop(op->type) ? 1
            : midsema_is_binop(op->type) ? 2
                                         : 3,
            midsema_is_unaryop(op->type) ? "operand" : "operands", out->len),
        .err = MIDDIAG_ERR_INSUFFICIENT_OPERANDS,
        .type = MIDDIAG_TYPE_ERROR};
}

static void add_op_to_out(struct midpar_Expr *op, struct midpar_ExprVec *out,
                          struct mid_DiagVec *diags)
{
    if (!has_enough_operands(op->type, out->len)) {
        midgen_dynpush(diags, not_enough_operands_err(op, out));
        return;
    }

    // the exprs at the top act as operands for the new op
    if (midsema_is_ternaryop(op->type))
        midgen_dynpush(&op->info.args, out->arr[out->len - 3]);

    bool real_bin_op =
        midsema_is_binop(op->type) && op->type != MIDPAR_EXPRTYPE_ARRAY_SUBSCR;
    if (midsema_is_ternaryop(op->type) || real_bin_op)
        midgen_dynpush(&op->info.args, out->arr[out->len - 2]);

    // func calls and array subscripts already have the arguments pushed into
    // args, so we gotta use insert to put the lhs first in the arg list
    bool push_top =
        !(op->type == MIDPAR_EXPRTYPE_FUNC_CALL && op->info.args.len > 0) &&
        !(op->type == MIDPAR_EXPRTYPE_ARRAY_SUBSCR);
    if (!push_top)
        midgen_dyninsert(&op->info.args, 0, out->arr[out->len - 1]);
    else
        midgen_dynpush(&op->info.args, out->arr[out->len - 1]);

    // the expressions are now encoded in op
    if (midsema_is_ternaryop(op->type))
        midgen_dynpop(out);
    if (midsema_is_ternaryop(op->type) || real_bin_op)
        midgen_dynpop(out);
    midgen_dynpop(out);

    midgen_dynpush(out, *op);
}

// handles sending an operator through the shunting yard
static void push_operator(midlex_TokenIter tok, midlex_TokenIter *out_end,
                          struct midpar_ExprVec *out,
                          struct midpar_ExprVec *ops, bool mode,
                          struct midsema_Scope *scope,
                          struct mid_DiagVec *diags)
{
    struct midpar_Expr op = op_tok_to_expr(tok, out_end, mode, scope, diags);

    // remove any greater precedence operators
    struct midpar_Expr *top = &ops->arr[ops->len - 1];
    while (ops->len > 0) {
        int32_t op_prec = midsema_op_precedence(op.type);
        int32_t top_prec = midsema_op_precedence(top->type);

        if (top_prec > op_prec ||
            (top_prec == op_prec && midsema_op_ltr_assoc(op.type))) {
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
static struct midpar_Expr parse_subexpr(const struct midlex_Token *l_paren,
                                        const struct midlex_Token **out_end,
                                        struct midsema_Scope *scope,
                                        struct mid_DiagVec *diags)
{
    if (midpar_find_twin_paren(l_paren, nullptr) == nullptr) {
        struct mid_Diag err = {.pos = l_paren->pos,
                               .line = l_paren->line,
                               .msg = midcmd_fmt_to_str("expected ')'"),
                               .err = MIDDIAG_ERR_MISSING_PAREN,
                               .type = MIDDIAG_TYPE_ERROR};
        midgen_dynpush(diags, err);
    }

    return midpar_parse_expr(
        l_paren + 1, (enum midlex_TokenType[]){MIDLEX_TOKENTYPE_R_PAREN}, 1,
        out_end, scope, diags);
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

struct midpar_Expr midpar_parse_expr(midlex_TokenIter start,
                                     const enum midlex_TokenType *end_types,
                                     mid_isize n_end_types,
                                     midlex_TokenIter *out_end,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags)
{
    // uses the shunting yard algorithm

    struct midpar_ExprVec out = midgen_dyninit();
    struct midpar_ExprVec ops = midgen_dyninit();

    // when false, binary operators remain binary and unary operators are
    // treated as postfix operators
    // when true, binary operators are treated as unary and unary operators are
    // treated as prefix operators
    // becomes true after finding an operand, becomes false after finding an
    // operator unless it's a unary postfix operator
    bool mode = true;

    midlex_TokenIter i;
    for (i = start; !is_end_type(i->type, end_types, n_end_types); ++i) {
        if (midlex_is_lit(i->type)) {
            if (!mode)
                midgen_dynpush(diags,
                               middiag_unexpected_token_err(
                                   "literal", i, MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                midgen_dynpush(&out, lit_tok_to_expr(i));
            mode = false;
        } else if (i->type == MIDLEX_TOKENTYPE_IDENTIFIER) {
            if (!mode)
                midgen_dynpush(
                    diags, middiag_unexpected_token_err(
                               "identifier", i, MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                midgen_dynpush(&out, ident_tok_to_expr(i));
            mode = false;
        } else if (i->type == MIDLEX_TOKENTYPE_THIS) {
            if (!mode)
                midgen_dynpush(diags,
                               middiag_unexpected_token_err(
                                   "this", i, MIDDIAG_ERR_UNEXPECTED_TOKEN));
            else
                midgen_dynpush(&out, this_tok_to_expr(i));
            mode = false;
        } else if (midlex_is_op(i->type)) {
            push_operator(i, &i, &out, &ops, mode, scope, diags);
            mode = ops.arr[ops.len - 1].type != MIDPAR_EXPRTYPE_POSTFIX_DEC &&
                   ops.arr[ops.len - 1].type != MIDPAR_EXPRTYPE_POSTFIX_INC;
        } else if (i->type == MIDLEX_TOKENTYPE_L_PAREN) {
            if (!mode)
                // if mode is 0, a sub-expression is actually a function call
                push_operator(i, &i, &out, &ops, mode, scope, diags);
            else
                midgen_dynpush(&out, parse_subexpr(i, &i, scope, diags));
            mode = false;
        } else if (i->type == MIDLEX_TOKENTYPE_L_SQBRACKET) {
            // array subscript operator
            push_operator(i, &i, &out, &ops, mode, scope, diags);
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
        printf("expr start at %d:%d\n", start->pos.line, start->pos.column);
        printf("expr end at %d:%d\n", i->pos.line, i->pos.column);
        printf("out len = %" MID_PRIisz "\n", out.len);
        MID_CRASH("mismatched operators and operands");
    }

    struct midpar_Expr ret = out.arr[0];
    midgen_dyndeinit(&out);
    midsema_typecheck_expr(&ret, scope, diags);
    return ret;
}

midlex_TokenIter midpar_skip_expr(midlex_TokenIter start,
                                  const enum midlex_TokenType *end_types,
                                  mid_isize n_end_types,
                                  struct mid_DiagVec *diags)
{
    midlex_TokenIter i;
    for (i = start; !is_end_type(i->type, end_types, n_end_types); ++i) {
        if (i->type == MIDLEX_TOKENTYPE_L_PAREN) {
            auto rparen = midpar_find_twin_paren(i, nullptr);
            if (rparen == nullptr && diags)
                midgen_dynpush(diags, middiag_expected_token_err(
                                          "')'", i, MIDDIAG_ERR_MISSING_PAREN));
            i = rparen == nullptr ? i : rparen;
        } else if (i->type == MIDLEX_TOKENTYPE_L_CURLY) {
            auto rcurly = midpar_find_twin_curly(i, nullptr);
            if (rcurly == nullptr && diags)
                midgen_dynpush(diags, middiag_expected_token_err(
                                          "'}'", i, MIDDIAG_ERR_MISSING_CURLY));
            i = rcurly == nullptr ? i : rcurly;
        } else if (i->type == MIDLEX_TOKENTYPE_L_SQBRACKET) {
            auto rsqbracket = midpar_find_twin_sqbracket(i, nullptr);
            if (rsqbracket == nullptr && diags)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "']'", i, MIDDIAG_ERR_MISSING_SQBRACKET));
            i = rsqbracket == nullptr ? i : rsqbracket;
        } else if (i->type == MIDLEX_TOKENTYPE_LT) {
            auto rangle = midpar_find_twin_angle(i, nullptr);
            if (rangle == nullptr && diags)
                midgen_dynpush(diags, middiag_expected_token_err(
                                          "'>'", i, MIDDIAG_ERR_MISSING_ANGLE));
            i = rangle == nullptr ? i : rangle;
        }
    }

    return i;
}

void midpar_Expr_deinit(struct midpar_Expr *expr)
{
    if (midsema_expr_uses_args(expr->type)) {
        for (mid_isize i = 0; i < expr->info.args.len; ++i)
            midpar_Expr_deinit(&expr->info.args.arr[i]);
        midgen_dyndeinit(&expr->info.args);
    } else if (midsema_is_lit(expr->type) ||
               expr->type == MIDPAR_EXPRTYPE_CONST_FOLD) {
        midlit_TaggedValue_deinit(&expr->info.val);
    }

    midpar_Type_deinit(&expr->ret);
}

struct midpar_Expr midpar_copy_expr(const struct midpar_Expr *expr)
{
    struct midpar_Expr ret = *expr;

    ret.ret = midpar_copy_type(&expr->ret);

    if (midsema_expr_uses_args(expr->type)) {
        ret.info.args = (struct midpar_ExprVec){};
        midgen_dynreserve(&ret.info.args, expr->info.args.len);
        for (mid_isize i = 0; i < expr->info.args.len; ++i)
            midgen_dynpush(&ret.info.args,
                           midpar_copy_expr(&expr->info.args.arr[i]));
    }

    return ret;
}
