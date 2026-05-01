#include "expr.h"
#include "diag.h"
#include "find_twin.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "macros.h"
#include "parser/end_types.h"
#include "parser/type.h"
#include "print.h"
#include <assert.h>
#include <stdio.h>

static struct Diag expected_token(const char *tok_name,
                                  const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("expected %s", tok_name),
        .err = ERRORTYPE_UNEXPECTED_TOKEN,
        .is_err = true,
    };
}

static struct Diag unexpected_token(const char *tok_name,
                                    const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("unexpected %s", tok_name),
        .err = ERRORTYPE_UNEXPECTED_TOKEN,
        .is_err = true,
    };
}

bool Parser_is_numlit(enum Parser_ExprType type)
{
    return type > PARSER_EXPRTYPE_NUMLIT_START &&
           type < PARSER_EXPRTYPE_NUMLIT_END;
}

bool Parser_is_ternaryop(enum Parser_ExprType type)
{
    return type > PARSER_EXPRTYPE_TERNARYOP_START &&
           type < PARSER_EXPRTYPE_TERNARYOP_END;
}

bool Parser_is_binop(enum Parser_ExprType type)
{
    return type > PARSER_EXPRTYPE_BINOP_START &&
           type < PARSER_EXPRTYPE_BINOP_END;
}

bool Parser_is_unaryop(enum Parser_ExprType type)
{
    return type > PARSER_EXPRTYPE_UNARYOP_START &&
           type < PARSER_EXPRTYPE_UNARYOP_END;
}

bool Parser_is_op(enum Parser_ExprType type)
{
    return Parser_is_binop(type) || Parser_is_unaryop(type);
}

i32 Parser_op_precedence(enum Parser_ExprType op)
{
    // goes from 16 to 1
    i32 flipped;

    switch (op) {
    case PARSER_EXPRTYPE_SCOPE_RES:
        flipped = 1;
        break;

    case PARSER_EXPRTYPE_MEMB_SEL:
    case PARSER_EXPRTYPE_PTR_MEMB_SEL:
    case PARSER_EXPRTYPE_ARRAY_SUBSCR:
    case PARSER_EXPRTYPE_FUNC_CALL:
    case PARSER_EXPRTYPE_POSTFIX_INC:
    case PARSER_EXPRTYPE_POSTFIX_DEC:
    case PARSER_EXPRTYPE_TYPEID:
    case PARSER_EXPRTYPE_CONSTCAST:
    case PARSER_EXPRTYPE_DYNAMICCAST:
    case PARSER_EXPRTYPE_REINTERPRETCAST:
    case PARSER_EXPRTYPE_STATICCAST:
        flipped = 2;
        break;

    case PARSER_EXPRTYPE_SIZEOF:
    case PARSER_EXPRTYPE_PREFIX_INC:
    case PARSER_EXPRTYPE_PREFIX_DEC:
    case PARSER_EXPRTYPE_BITWISE_NOT:
    case PARSER_EXPRTYPE_LOGICAL_NOT:
    case PARSER_EXPRTYPE_UNARY_MINUS:
    case PARSER_EXPRTYPE_UNARY_PLUS:
    case PARSER_EXPRTYPE_REF:
    case PARSER_EXPRTYPE_DEREF:
    case PARSER_EXPRTYPE_NEW:
    case PARSER_EXPRTYPE_DELETE:
    case PARSER_EXPRTYPE_CAST:
        flipped = 3;
        break;

    case PARSER_EXPRTYPE_PTR_TO_MEMB_SEL:
    case PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        flipped = 4;
        break;

    case PARSER_EXPRTYPE_MUL:
    case PARSER_EXPRTYPE_DIV:
    case PARSER_EXPRTYPE_MOD:
        flipped = 5;
        break;

    case PARSER_EXPRTYPE_ADD:
    case PARSER_EXPRTYPE_SUB:
        flipped = 6;
        break;

    case PARSER_EXPRTYPE_LEFT_SHIFT:
    case PARSER_EXPRTYPE_RIGHT_SHIFT:
        flipped = 7;
        break;

    case PARSER_EXPRTYPE_LT:
    case PARSER_EXPRTYPE_GT:
    case PARSER_EXPRTYPE_LTEQ:
    case PARSER_EXPRTYPE_GTEQ:
        flipped = 8;
        break;

    case PARSER_EXPRTYPE_EQ:
    case PARSER_EXPRTYPE_NEQ:
        flipped = 9;
        break;

    case PARSER_EXPRTYPE_BITWISE_AND:
        flipped = 10;
        break;

    case PARSER_EXPRTYPE_BITWISE_XOR:
        flipped = 11;
        break;

    case PARSER_EXPRTYPE_BITWISE_OR:
        flipped = 12;
        break;

    case PARSER_EXPRTYPE_LOGICAL_AND:
        flipped = 13;
        break;

    case PARSER_EXPRTYPE_LOGICAL_OR:
        flipped = 14;
        break;

    case PARSER_EXPRTYPE_CONDITIONAL:
    case PARSER_EXPRTYPE_ASSIGN:
    case PARSER_EXPRTYPE_MUL_ASSIGN:
    case PARSER_EXPRTYPE_DIV_ASSIGN:
    case PARSER_EXPRTYPE_MOD_ASSIGN:
    case PARSER_EXPRTYPE_ADD_ASSIGN:
    case PARSER_EXPRTYPE_SUB_ASSIGN:
    case PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
    case PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
    case PARSER_EXPRTYPE_AND_ASSIGN:
    case PARSER_EXPRTYPE_XOR_ASSIGN:
    case PARSER_EXPRTYPE_OR_ASSIGN:
    case PARSER_EXPRTYPE_THROW:
        flipped = 15;
        break;

    default:
        CRASH("expr isn't an operator");
    }

    return 16 - flipped;
}

bool Parser_op_ltr_assoc(enum Parser_ExprType op)
{
    i32 prec = Parser_op_precedence(op);
    return prec != 13 && prec != 1;
}

bool Parser_is_glvalue(enum Parser_ExprValueType type)
{
    return type == PARSER_EXPRVALUE_LVALUE || type == PARSER_EXPRVALUE_XVALUE;
}

bool Parser_is_rvalue(enum Parser_ExprValueType type)
{
    return type == PARSER_EXPRVALUE_PRVALUE || type == PARSER_EXPRVALUE_XVALUE;
}

bool Parser_is_rvalue(enum Parser_ExprValueType type);

static struct Parser_Expr numlit_tok_to_expr(const struct Lexer_Token *tok)
{
    assert(Lexer_is_numlit(tok->type));

    struct Parser_Expr ret = {
        .tok = tok, .info.val = tok->val, .valtype = PARSER_EXPRVALUE_PRVALUE};

    switch (tok->type) {
    case LEXER_TOKENTYPE_INT_LIT:
        ret.type = PARSER_EXPRTYPE_INT_LIT;
        break;

    case LEXER_TOKENTYPE_UINT_LIT:
        ret.type = PARSER_EXPRTYPE_UINT_LIT;
        break;

    case LEXER_TOKENTYPE_LONG_LIT:
        ret.type = PARSER_EXPRTYPE_LONG_LIT;
        break;

    case LEXER_TOKENTYPE_ULONG_LIT:
        ret.type = PARSER_EXPRTYPE_ULONG_LIT;
        break;

    case LEXER_TOKENTYPE_LONGLONG_LIT:
        ret.type = PARSER_EXPRTYPE_LONGLONG_LIT;
        break;

    case LEXER_TOKENTYPE_ULONGLONG_LIT:
        ret.type = PARSER_EXPRTYPE_ULONGLONG_LIT;
        break;

    case LEXER_TOKENTYPE_FLOAT_LIT:
        ret.type = PARSER_EXPRTYPE_FLOAT_LIT;
        break;

    case LEXER_TOKENTYPE_DOUBLE_LIT:
        ret.type = PARSER_EXPRTYPE_DOUBLE_LIT;
        break;

    case LEXER_TOKENTYPE_LONGDOUBLE_LIT:
        ret.type = PARSER_EXPRTYPE_LONGDOUBLE_LIT;
        break;

    case LEXER_TOKENTYPE_BOOL_LIT:
        ret.type = PARSER_EXPRTYPE_BOOL_LIT;
        break;

    case LEXER_TOKENTYPE_PTR_LIT:
        ret.type = PARSER_EXPRTYPE_PTR_LIT;
        break;

    default:
        assert(false);
        break;
    }

    return ret;
}

static struct Parser_Expr ident_tok_to_expr(const struct Lexer_Token *tok)
{
    assert(tok->type == LEXER_TOKENTYPE_IDENTIFIER);

    struct Parser_Expr ret = {.tok = tok,
                              .info.ident = tok->ident,
                              .type = PARSER_EXPRTYPE_IDENTIFIER};
    return ret;
}

static struct Parser_Expr op_tok_to_expr_mode0(const struct Lexer_Token *tok,
                                               struct DiagVec *diags)
{
    struct Parser_Expr ret = {.tok = tok};

    switch (tok->type) {
    case LEXER_TOKENTYPE_SCOPE_RES:
        ret.type = PARSER_EXPRTYPE_SCOPE_RES;
        break;

    case LEXER_TOKENTYPE_MEMB_SEL:
        ret.type = PARSER_EXPRTYPE_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_PTR_MEMB_SEL:
        ret.type = PARSER_EXPRTYPE_PTR_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_L_SQBRACKET:
        ret.type = PARSER_EXPRTYPE_ARRAY_SUBSCR;
        break;

    case LEXER_TOKENTYPE_L_PAREN:
        ret.type = PARSER_EXPRTYPE_FUNC_CALL;
        break;

    case LEXER_TOKENTYPE_INC:
        ret.type = PARSER_EXPRTYPE_POSTFIX_INC;
        break;

    case LEXER_TOKENTYPE_DEC:
        ret.type = PARSER_EXPRTYPE_POSTFIX_DEC;
        break;

    case LEXER_TOKENTYPE_TYPEID:
        gen_dynpush(diags, unexpected_token("typeid", tok));
        ret.type = PARSER_EXPRTYPE_TYPEID;
        break;

    case LEXER_TOKENTYPE_CONSTCAST:
        gen_dynpush(diags, unexpected_token("const_cast", tok));
        ret.type = PARSER_EXPRTYPE_CONSTCAST;
        break;

    case LEXER_TOKENTYPE_DYNAMICCAST:
        gen_dynpush(diags, unexpected_token("dynamic_cast", tok));
        ret.type = PARSER_EXPRTYPE_DYNAMICCAST;
        break;

    case LEXER_TOKENTYPE_REINTERPRETCAST:
        gen_dynpush(diags, unexpected_token("reinterpret_cast", tok));
        ret.type = PARSER_EXPRTYPE_REINTERPRETCAST;
        break;

    case LEXER_TOKENTYPE_STATICCAST:
        gen_dynpush(diags, unexpected_token("static_cast", tok));
        ret.type = PARSER_EXPRTYPE_STATICCAST;
        break;

    case LEXER_TOKENTYPE_SIZEOF:
        gen_dynpush(diags, unexpected_token("sizeof", tok));
        ret.type = PARSER_EXPRTYPE_SIZEOF;
        break;

    case LEXER_TOKENTYPE_BITWISE_NOT:
        gen_dynpush(diags, unexpected_token("bitwise not '~'", tok));
        ret.type = PARSER_EXPRTYPE_BITWISE_NOT;
        break;

    case LEXER_TOKENTYPE_LOGICAL_NOT:
        gen_dynpush(diags, unexpected_token("logical not '!'", tok));
        ret.type = PARSER_EXPRTYPE_LOGICAL_NOT;
        break;

    case LEXER_TOKENTYPE_SUB:
        ret.type = PARSER_EXPRTYPE_SUB;
        break;

    case LEXER_TOKENTYPE_ADD:
        ret.type = PARSER_EXPRTYPE_ADD;
        break;

    case LEXER_TOKENTYPE_BITWISE_AND:
        ret.type = PARSER_EXPRTYPE_BITWISE_AND;
        break;

    case LEXER_TOKENTYPE_MUL:
        ret.type = PARSER_EXPRTYPE_MUL;
        break;

    case LEXER_TOKENTYPE_NEW:
        gen_dynpush(diags, unexpected_token("new", tok));
        ret.type = PARSER_EXPRTYPE_NEW;
        break;

    case LEXER_TOKENTYPE_DELETE:
        gen_dynpush(diags, unexpected_token("delete", tok));
        ret.type = PARSER_EXPRTYPE_DELETE;
        break;

    case LEXER_TOKENTYPE_PTR_TO_MEMB_SEL:
        ret.type = PARSER_EXPRTYPE_PTR_TO_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        ret.type = PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_DIV:
        ret.type = PARSER_EXPRTYPE_DIV;
        break;

    case LEXER_TOKENTYPE_MOD:
        ret.type = PARSER_EXPRTYPE_MOD;
        break;

    case LEXER_TOKENTYPE_LEFT_SHIFT:
        ret.type = PARSER_EXPRTYPE_LEFT_SHIFT;
        break;

    case LEXER_TOKENTYPE_RIGHT_SHIFT:
        ret.type = PARSER_EXPRTYPE_RIGHT_SHIFT;
        break;

    case LEXER_TOKENTYPE_LT:
        ret.type = PARSER_EXPRTYPE_LT;
        break;

    case LEXER_TOKENTYPE_GT:
        ret.type = PARSER_EXPRTYPE_GT;
        break;

    case LEXER_TOKENTYPE_LTEQ:
        ret.type = PARSER_EXPRTYPE_LTEQ;
        break;

    case LEXER_TOKENTYPE_GTEQ:
        ret.type = PARSER_EXPRTYPE_GTEQ;
        break;

    case LEXER_TOKENTYPE_EQ:
        ret.type = PARSER_EXPRTYPE_EQ;
        break;

    case LEXER_TOKENTYPE_NEQ:
        ret.type = PARSER_EXPRTYPE_NEQ;
        break;

    case LEXER_TOKENTYPE_BITWISE_XOR:
        ret.type = PARSER_EXPRTYPE_BITWISE_XOR;
        break;

    case LEXER_TOKENTYPE_BITWISE_OR:
        ret.type = PARSER_EXPRTYPE_BITWISE_OR;
        break;

    case LEXER_TOKENTYPE_LOGICAL_AND:
        ret.type = PARSER_EXPRTYPE_LOGICAL_AND;
        break;

    case LEXER_TOKENTYPE_LOGICAL_OR:
        ret.type = PARSER_EXPRTYPE_LOGICAL_OR;
        break;

    case LEXER_TOKENTYPE_CONDITIONAL:
        ret.type = PARSER_EXPRTYPE_CONDITIONAL;
        break;

    case LEXER_TOKENTYPE_ASSIGN:
        ret.type = PARSER_EXPRTYPE_ASSIGN;
        break;

    case LEXER_TOKENTYPE_MUL_ASSIGN:
        ret.type = PARSER_EXPRTYPE_MUL_ASSIGN;
        break;

    case LEXER_TOKENTYPE_DIV_ASSIGN:
        ret.type = PARSER_EXPRTYPE_DIV_ASSIGN;
        break;

    case LEXER_TOKENTYPE_MOD_ASSIGN:
        ret.type = PARSER_EXPRTYPE_MOD_ASSIGN;
        break;

    case LEXER_TOKENTYPE_ADD_ASSIGN:
        ret.type = PARSER_EXPRTYPE_ADD_ASSIGN;
        break;

    case LEXER_TOKENTYPE_SUB_ASSIGN:
        ret.type = PARSER_EXPRTYPE_SUB_ASSIGN;
        break;

    case LEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        ret.type = PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;
        break;

    case LEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        ret.type = PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;
        break;

    case LEXER_TOKENTYPE_AND_ASSIGN:
        ret.type = PARSER_EXPRTYPE_AND_ASSIGN;
        break;

    case LEXER_TOKENTYPE_OR_ASSIGN:
        ret.type = PARSER_EXPRTYPE_OR_ASSIGN;
        break;

    case LEXER_TOKENTYPE_XOR_ASSIGN:
        ret.type = PARSER_EXPRTYPE_XOR_ASSIGN;
        break;

    case LEXER_TOKENTYPE_THROW:
        gen_dynpush(diags, unexpected_token("throw", tok));
        ret.type = PARSER_EXPRTYPE_THROW;
        break;

    case LEXER_TOKENTYPE_COMMA:
        ret.type = PARSER_EXPRTYPE_COMMA;
        break;

    default:
        CRASH("can't convert token to expr");
    }

    return ret;
}

static struct Parser_Expr op_tok_to_expr_mode1(const struct Lexer_Token *tok,
                                               struct DiagVec *diags)
{
    struct Parser_Expr ret = {.tok = tok};

    switch (tok->type) {
    case LEXER_TOKENTYPE_SCOPE_RES:
        gen_dynpush(diags, unexpected_token("scope resolution '::'", tok));
        ret.type = PARSER_EXPRTYPE_SCOPE_RES;
        break;

    case LEXER_TOKENTYPE_MEMB_SEL:
        gen_dynpush(diags, unexpected_token("member select '.'", tok));
        ret.type = PARSER_EXPRTYPE_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_PTR_MEMB_SEL:
        gen_dynpush(diags, unexpected_token("ptr to member select '->'", tok));
        ret.type = PARSER_EXPRTYPE_PTR_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_L_SQBRACKET:
        gen_dynpush(diags, unexpected_token("array subscript '[]'", tok));
        ret.type = PARSER_EXPRTYPE_ARRAY_SUBSCR;
        break;

    case LEXER_TOKENTYPE_L_PAREN:
        gen_dynpush(diags, unexpected_token("function call '()'", tok));
        ret.type = PARSER_EXPRTYPE_FUNC_CALL;
        break;

    case LEXER_TOKENTYPE_INC:
        ret.type = PARSER_EXPRTYPE_PREFIX_INC;
        break;

    case LEXER_TOKENTYPE_DEC:
        ret.type = PARSER_EXPRTYPE_PREFIX_DEC;
        break;

    case LEXER_TOKENTYPE_TYPEID:
        ret.type = PARSER_EXPRTYPE_TYPEID;
        break;

    case LEXER_TOKENTYPE_CONSTCAST:
        ret.type = PARSER_EXPRTYPE_CONSTCAST;
        break;

    case LEXER_TOKENTYPE_DYNAMICCAST:
        ret.type = PARSER_EXPRTYPE_DYNAMICCAST;
        break;

    case LEXER_TOKENTYPE_REINTERPRETCAST:
        ret.type = PARSER_EXPRTYPE_REINTERPRETCAST;
        break;

    case LEXER_TOKENTYPE_STATICCAST:
        ret.type = PARSER_EXPRTYPE_STATICCAST;
        break;

    case LEXER_TOKENTYPE_SIZEOF:
        ret.type = PARSER_EXPRTYPE_SIZEOF;
        break;

    case LEXER_TOKENTYPE_BITWISE_NOT:
        ret.type = PARSER_EXPRTYPE_BITWISE_NOT;
        break;

    case LEXER_TOKENTYPE_LOGICAL_NOT:
        ret.type = PARSER_EXPRTYPE_LOGICAL_NOT;
        break;

    case LEXER_TOKENTYPE_SUB:
        ret.type = PARSER_EXPRTYPE_UNARY_MINUS;
        break;

    case LEXER_TOKENTYPE_ADD:
        ret.type = PARSER_EXPRTYPE_UNARY_PLUS;
        break;

    case LEXER_TOKENTYPE_BITWISE_AND:
        ret.type = PARSER_EXPRTYPE_REF;
        break;

    case LEXER_TOKENTYPE_MUL:
        ret.type = PARSER_EXPRTYPE_DEREF;
        break;

    case LEXER_TOKENTYPE_NEW:
        ret.type = PARSER_EXPRTYPE_NEW;
        break;

    case LEXER_TOKENTYPE_DELETE:
        ret.type = PARSER_EXPRTYPE_DELETE;
        break;

    case LEXER_TOKENTYPE_PTR_TO_MEMB_SEL:
        gen_dynpush(diags, unexpected_token("ptr to member select '.*'", tok));
        ret.type = PARSER_EXPRTYPE_PTR_TO_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        gen_dynpush(diags,
                    unexpected_token("ptr to ptr member select '->*'", tok));
        ret.type = PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
        break;

    case LEXER_TOKENTYPE_DIV:
        gen_dynpush(diags, unexpected_token("division '/'", tok));
        ret.type = PARSER_EXPRTYPE_DIV;
        break;

    case LEXER_TOKENTYPE_MOD:
        gen_dynpush(diags, unexpected_token("modulo '%'", tok));
        ret.type = PARSER_EXPRTYPE_MOD;
        break;

    case LEXER_TOKENTYPE_LEFT_SHIFT:
        gen_dynpush(diags, unexpected_token("left shift '<<'", tok));
        ret.type = PARSER_EXPRTYPE_LEFT_SHIFT;
        break;

    case LEXER_TOKENTYPE_RIGHT_SHIFT:
        gen_dynpush(diags, unexpected_token("right shift '>>'", tok));
        ret.type = PARSER_EXPRTYPE_RIGHT_SHIFT;
        break;

    case LEXER_TOKENTYPE_LT:
        gen_dynpush(diags, unexpected_token("less than '<'", tok));
        ret.type = PARSER_EXPRTYPE_LT;
        break;

    case LEXER_TOKENTYPE_GT:
        gen_dynpush(diags, unexpected_token("greater than '>'", tok));
        ret.type = PARSER_EXPRTYPE_GT;
        break;

    case LEXER_TOKENTYPE_LTEQ:
        gen_dynpush(diags, unexpected_token("less than or equal '<='", tok));
        ret.type = PARSER_EXPRTYPE_LTEQ;
        break;

    case LEXER_TOKENTYPE_GTEQ:
        gen_dynpush(diags, unexpected_token("greater than or equal '>='", tok));
        ret.type = PARSER_EXPRTYPE_GTEQ;
        break;

    case LEXER_TOKENTYPE_EQ:
        gen_dynpush(diags, unexpected_token("equality '=='", tok));
        ret.type = PARSER_EXPRTYPE_EQ;
        break;

    case LEXER_TOKENTYPE_NEQ:
        gen_dynpush(diags, unexpected_token("inequality '!='", tok));
        ret.type = PARSER_EXPRTYPE_NEQ;
        break;

    case LEXER_TOKENTYPE_BITWISE_XOR:
        gen_dynpush(diags, unexpected_token("bitwise XOR '^'", tok));
        ret.type = PARSER_EXPRTYPE_BITWISE_XOR;
        break;

    case LEXER_TOKENTYPE_BITWISE_OR:
        gen_dynpush(diags, unexpected_token("bitwise OR '|'", tok));
        ret.type = PARSER_EXPRTYPE_BITWISE_OR;
        break;

    case LEXER_TOKENTYPE_LOGICAL_AND:
        gen_dynpush(diags, unexpected_token("logical AND '&&'", tok));
        ret.type = PARSER_EXPRTYPE_LOGICAL_AND;
        break;

    case LEXER_TOKENTYPE_LOGICAL_OR:
        gen_dynpush(diags, unexpected_token("logical OR '||'", tok));
        ret.type = PARSER_EXPRTYPE_LOGICAL_OR;
        break;

    case LEXER_TOKENTYPE_CONDITIONAL:
        gen_dynpush(diags, unexpected_token("conditional operator '?'", tok));
        ret.type = PARSER_EXPRTYPE_CONDITIONAL;
        break;

    case LEXER_TOKENTYPE_ASSIGN:
        gen_dynpush(diags, unexpected_token("assignment '=='", tok));
        ret.type = PARSER_EXPRTYPE_ASSIGN;
        break;

    case LEXER_TOKENTYPE_MUL_ASSIGN:
        gen_dynpush(diags,
                    unexpected_token("multiplication assignment '*='", tok));
        ret.type = PARSER_EXPRTYPE_MUL_ASSIGN;
        break;

    case LEXER_TOKENTYPE_DIV_ASSIGN:
        gen_dynpush(diags, unexpected_token("division assignment '/='", tok));
        ret.type = PARSER_EXPRTYPE_DIV_ASSIGN;
        break;

    case LEXER_TOKENTYPE_MOD_ASSIGN:
        gen_dynpush(diags, unexpected_token("modulus assignment '%='", tok));
        ret.type = PARSER_EXPRTYPE_MOD_ASSIGN;
        break;

    case LEXER_TOKENTYPE_ADD_ASSIGN:
        gen_dynpush(diags, unexpected_token("addition assignment '+='", tok));
        ret.type = PARSER_EXPRTYPE_ADD_ASSIGN;
        break;

    case LEXER_TOKENTYPE_SUB_ASSIGN:
        gen_dynpush(diags,
                    unexpected_token("subtraction assignment '-='", tok));
        ret.type = PARSER_EXPRTYPE_SUB_ASSIGN;
        break;

    case LEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        gen_dynpush(diags,
                    unexpected_token("left shift assignment '<<='", tok));
        ret.type = PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;
        break;

    case LEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        gen_dynpush(diags,
                    unexpected_token("right shift assignment '>>='", tok));
        ret.type = PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;
        break;

    case LEXER_TOKENTYPE_AND_ASSIGN:
        gen_dynpush(diags,
                    unexpected_token("bitwise AND assignment '&='", tok));
        ret.type = PARSER_EXPRTYPE_AND_ASSIGN;
        break;

    case LEXER_TOKENTYPE_OR_ASSIGN:
        gen_dynpush(diags, unexpected_token("bitwise OR assignment '|='", tok));
        ret.type = PARSER_EXPRTYPE_OR_ASSIGN;
        break;

    case LEXER_TOKENTYPE_XOR_ASSIGN:
        gen_dynpush(diags,
                    unexpected_token("bitwise XOR assignment '^='", tok));
        ret.type = PARSER_EXPRTYPE_XOR_ASSIGN;
        break;

    case LEXER_TOKENTYPE_THROW:
        ret.type = PARSER_EXPRTYPE_THROW;
        break;

    case LEXER_TOKENTYPE_COMMA:
        gen_dynpush(diags, unexpected_token("comma ','", tok));
        ret.type = PARSER_EXPRTYPE_COMMA;
        break;

    default:
        CRASH("can't convert token to expr");
    }

    return ret;
}

static void parse_func_call_args(struct Parser_Expr *f_call,
                                 const struct Lexer_Token *toks, isize_t lparen,
                                 isize_t *out_rparen, struct DiagVec *diags)
{
    isize_t rparen = Parser_find_twin_paren(toks, lparen, ISIZE_MAX);
    if (rparen == -1) {
        gen_dynpush(diags, expected_token("')'", &toks[lparen]));
        rparen = lparen;
    }
    if (out_rparen)
        *out_rparen = rparen;

    for (isize_t i = lparen + 1; i < rparen; ++i) {
        auto arg = Parser_parse_expr(toks, i, PARSER_PARAM_ENDTYPES, &i, diags);
        gen_dynpush(&f_call->info.args, arg);

        if (toks[i].type != LEXER_TOKENTYPE_R_PAREN &&
            toks[i].type != LEXER_TOKENTYPE_COMMA) {
            gen_dynpush(diags, expected_token("')'", &toks[lparen]));
        }
    }
}

// in most cases out_end_idx is set to idx, but in some cases like func calls
// and array subscripts out_end_idx is set to the last token in the expr:
// func(a, b, c, d)
//     ^          ^
//    idx    out_end_idx
static struct Parser_Expr op_tok_to_expr(const struct Lexer_Token *toks,
                                         isize_t idx, isize_t *out_end_idx,
                                         bool mode, struct DiagVec *diags)
{
    struct Parser_Expr ret = mode ? op_tok_to_expr_mode1(&toks[idx], diags)
                                  : op_tok_to_expr_mode0(&toks[idx], diags);

    if (ret.type == PARSER_EXPRTYPE_FUNC_CALL)
        parse_func_call_args(&ret, toks, idx, out_end_idx, diags);
    else if (out_end_idx)
        *out_end_idx = idx;

    return ret;
}

static bool has_enough_operands(enum Parser_ExprType op, int n)
{
    if (Parser_is_unaryop(op))
        return n >= 1;
    else if (Parser_is_binop(op))
        return n >= 2;
    else if (Parser_is_ternaryop(op))
        return n >= 3;
    else
        assert(false);
}

static void add_op_to_out(struct Parser_Expr *op, struct Parser_ExprVec *out,
                          struct DiagVec *diags)
{
    if (!has_enough_operands(op->type, out->len)) {
        struct Diag err = {
            .pos = op->tok->pos,
            .line = op->tok->line,
            .msg = Print_fmt_to_str(
                "%s operator expects %d %s, received %" PRIisz,
                Parser_is_unaryop(op->type) ? "unary"
                : Parser_is_binop(op->type) ? "binary"
                                            : "ternary",
                Parser_is_unaryop(op->type) ? 1
                : Parser_is_binop(op->type) ? 2
                                            : 3,
                Parser_is_unaryop(op->type) ? "operand" : "operands", out->len),
            .err = ERRORTYPE_INSUFFICIENT_OPERANDS,
            .is_err = true};
        gen_dynpush(diags, err);
        return;
    }

    // the exprs at the top act as operands for the new op
    if (Parser_is_ternaryop(op->type))
        gen_dynpush(&op->info.args, out->arr[out->len - 3]);
    if (Parser_is_ternaryop(op->type) || Parser_is_binop(op->type))
        gen_dynpush(&op->info.args, out->arr[out->len - 2]);
    if (op->type == PARSER_EXPRTYPE_FUNC_CALL && op->info.args.len > 0)
        // func calls already have the arguments pushed into args, so we gotta
        // use insert to put the identifier being called first
        gen_dyninsert(&op->info.args, 0, out->arr[out->len - 1]);
    else
        gen_dynpush(&op->info.args, out->arr[out->len - 1]);

    // the expressions are now encoded in op
    if (Parser_is_ternaryop(op->type))
        gen_dynpop(out);
    if (Parser_is_ternaryop(op->type) || Parser_is_binop(op->type))
        gen_dynpop(out);
    gen_dynpop(out);

    gen_dynpush(out, *op);
}

// handles sending an operator through the shunting yard
static void push_operator(const struct Lexer_Token *toks, isize_t idx,
                          isize_t *out_end_idx, struct Parser_ExprVec *out,
                          struct Parser_ExprVec *ops, bool mode,
                          struct DiagVec *diags)
{
    struct Parser_Expr op = op_tok_to_expr(toks, idx, out_end_idx, mode, diags);

    // remove any greater precedence operators
    struct Parser_Expr *top = &ops->arr[ops->len - 1];
    while (ops->len > 0) {
        i32 op_prec = Parser_op_precedence(op.type);
        i32 top_prec = Parser_op_precedence(top->type);

        if (top_prec > op_prec ||
            (top_prec == op_prec && Parser_op_ltr_assoc(op.type))) {
            add_op_to_out(top, out, diags);
            gen_dynpop(ops);
            top = &ops->arr[ops->len - 1];
        } else {
            break;
        }
    }

    gen_dynpush(ops, op);
}

// a sub expression is a part of an expression encased in parentheses
static struct Parser_Expr parse_subexpr(const struct Lexer_Token *toks,
                                        isize_t l_paren, isize_t *out_end,
                                        struct DiagVec *diags)
{
    if (Parser_find_twin_paren(toks, l_paren, ISIZE_MAX) == -1) {
        struct Diag err = {.pos = toks[l_paren].pos,
                           .line = toks[l_paren].line,
                           .msg = Print_fmt_to_str("expected ')'"),
                           .err = ERRORTYPE_MISSING_PAREN,
                           .is_err = true};
        gen_dynpush(diags, err);
    }

    return Parser_parse_expr(toks, l_paren + 1,
                             (enum Lexer_TokenType[]){LEXER_TOKENTYPE_R_PAREN},
                             1, out_end, diags);
}

static bool is_end_type(enum Lexer_TokenType type,
                        const enum Lexer_TokenType *end_types,
                        isize_t n_end_types)
{
    for (isize_t i = 0; i < n_end_types; ++i)
        if (type == end_types[i])
            return true;
    return false;
}

struct Parser_Expr Parser_parse_expr(const struct Lexer_Token *toks,
                                     isize_t start,
                                     const enum Lexer_TokenType *end_types,
                                     isize_t n_end_types, isize_t *out_end,
                                     struct DiagVec *diags)
{
    // uses the shunting yard algorithm

    struct Parser_ExprVec out = gen_dyninit();
    struct Parser_ExprVec ops = gen_dyninit();

    // when false, binary operators remain binary and unary operators are
    // treated as postifx operators
    // when true, binary operators are treated as unary and unary operators are
    // treated as prefix operators
    // becomes true after finding an operand, becomes false after finding an
    // operator unless it's a unary postfix operator
    bool mode = true;

    isize_t i;
    for (i = start; !is_end_type(toks[i].type, end_types, n_end_types); ++i) {
        if (Lexer_is_numlit(toks[i].type)) {
            if (!mode)
                gen_dynpush(diags, unexpected_token("literal", &toks[i]));
            else
                gen_dynpush(&out, numlit_tok_to_expr(&toks[i]));
            mode = false;
        } else if (toks[i].type == LEXER_TOKENTYPE_IDENTIFIER) {
            if (!mode)
                gen_dynpush(diags, unexpected_token("identifier", &toks[i]));
            else
                gen_dynpush(&out, ident_tok_to_expr(&toks[i]));
            mode = false;
        } else if (Lexer_is_op(toks[i].type)) {
            push_operator(toks, i, &i, &out, &ops, mode, diags);
            mode = ops.arr[ops.len - 1].type != PARSER_EXPRTYPE_POSTFIX_DEC &&
                   ops.arr[ops.len - 1].type != PARSER_EXPRTYPE_POSTFIX_INC;
        } else if (toks[i].type == LEXER_TOKENTYPE_L_PAREN) {
            if (!mode)
                // if mode is 0 a sub-expression is actually a function call
                push_operator(toks, i, &i, &out, &ops, mode, diags);
            else
                gen_dynpush(&out, parse_subexpr(toks, i, &i, diags));
            mode = false;
        }
    }
    if (out_end)
        *out_end = i;

    // excess operators just get popped in fifo order
    while (ops.len > 0) {
        add_op_to_out(&ops.arr[ops.len - 1], &out, diags);
        gen_dynpop(&ops);
    }
    gen_dyndeinit(&ops);

    if (out.len != 1) {
        // handle operator and operand mismatch here
        printf("expr start at %d:%d\n", toks[start].pos.line,
               toks[start].pos.column);
        printf("expr end at %d:%d\n", toks[i].pos.line, toks[i].pos.column);
        printf("out len = %" PRIisz "\n", out.len);
        CRASH("mismatched operators and operands");
    }

    struct Parser_Expr ret = out.arr[0];
    gen_dyndeinit(&out);
    return ret;
}

static union Literal_Value evaluate_binop(const struct Parser_Expr *expr)
{
    assert(expr->info.args.len == 2);

    union Literal_Value vals[2];
    for (isize_t i = 0; i < 2; ++i) {
        vals[i] = Parser_evaluate(&expr->info.args.arr[i]);
    }

    switch (expr->type) {
    case PARSER_EXPRTYPE_ADD:
        return (union Literal_Value){.sint = vals[0].sint + vals[1].sint};

    case PARSER_EXPRTYPE_SUB:
        return (union Literal_Value){.sint = vals[0].sint - vals[1].sint};

    case PARSER_EXPRTYPE_MUL:
        return (union Literal_Value){.sint = vals[0].sint * vals[1].sint};

    case PARSER_EXPRTYPE_DIV:
        return (union Literal_Value){.sint = vals[0].sint / vals[1].sint};

    default:
        assert(false);
    }
}

union Literal_Value Parser_evaluate(const struct Parser_Expr *expr)
{
    if (Parser_is_numlit(expr->type))
        return expr->info.val;
    else if (Parser_is_unaryop(expr->type))
        assert(false);
    else if (Parser_is_binop(expr->type))
        return evaluate_binop(expr);
    else if (Parser_is_ternaryop(expr->type))
        assert(false);
    else
        assert(false);
}

void Parser_Expr_deinit(struct Parser_Expr *expr)
{
    if (Parser_expr_uses_args(expr->type)) {
        for (isize_t i = 0; i < expr->info.args.len; ++i)
            Parser_Expr_deinit(&expr->info.args.arr[i]);
        gen_dyndeinit(&expr->info.args);
    }

    Parser_Type_deinit(&expr->ret);
}

bool Parser_expr_uses_args(enum Parser_ExprType type)
{
    return Parser_is_op(type);
}
