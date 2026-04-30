#include "expr.h"
#include "diag.h"
#include "find_twin.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "macros.h"
#include "parser/type.h"
#include "print.h"
#include <assert.h>
#include <stdio.h>

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
    case PARSER_EXPRTYPE_ADD:
        flipped = 6;
        break;

    case PARSER_EXPRTYPE_SUB:
        flipped = 6;
        break;

    case PARSER_EXPRTYPE_MUL:
        flipped = 5;
        break;

    case PARSER_EXPRTYPE_DIV:
        flipped = 5;
        break;

    case PARSER_EXPRTYPE_ASSIGN:
        flipped = 15;
        break;

    case PARSER_EXPRTYPE_BITWISE_AND:
        flipped = 10;
        break;

    case PARSER_EXPRTYPE_LOGICAL_AND:
        flipped = 13;
        break;

    case PARSER_EXPRTYPE_COMMA:
        flipped = 16;
        break;

    case PARSER_EXPRTYPE_DEREF:
        flipped = 3;
        break;

    case PARSER_EXPRTYPE_REF:
        flipped = 3;
        break;

    default:
        assert(false);
        break;
    }

    return 16 - flipped;
}

bool Parser_op_ltr_assoc(enum Parser_ExprType op)
{
    i32 prec = Parser_op_precedence(op);
    return prec != 13 && prec != 1;
}

static struct Parser_Expr numlit_tok_to_expr(const struct Lexer_Token *tok)
{
    assert(Lexer_is_numlit(tok->type));

    struct Parser_Expr ret = {.tok = tok, .info.val = tok->val};

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

static struct Parser_Expr op_tok_to_expr(const struct Lexer_Token *tok)
{
    assert(Lexer_is_op(tok->type));

    struct Parser_Expr ret = {.tok = tok};

    switch (tok->type) {
    case LEXER_TOKENTYPE_ADD:
        ret.type = PARSER_EXPRTYPE_ADD;
        break;

    case LEXER_TOKENTYPE_SUB:
        ret.type = PARSER_EXPRTYPE_SUB;
        break;

    case LEXER_TOKENTYPE_MUL:
        ret.type = PARSER_EXPRTYPE_MUL;
        break;

    case LEXER_TOKENTYPE_DIV:
        ret.type = PARSER_EXPRTYPE_DIV;
        break;

    case LEXER_TOKENTYPE_ASSIGN:
        ret.type = PARSER_EXPRTYPE_ASSIGN;
        break;

    case LEXER_TOKENTYPE_BITWISE_AND:
        ret.type = PARSER_EXPRTYPE_BITWISE_AND;
        break;

    case LEXER_TOKENTYPE_LOGICAL_AND:
        ret.type = PARSER_EXPRTYPE_LOGICAL_AND;
        break;

    case LEXER_TOKENTYPE_COMMA:
        ret.type = PARSER_EXPRTYPE_COMMA;

    case LEXER_TOKENTYPE_DEREF:
        ret.type = PARSER_EXPRTYPE_DEREF;
        break;

    case LEXER_TOKENTYPE_REF:
        ret.type = PARSER_EXPRTYPE_REF;
        break;

    default:
        assert(false);
    }

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

    op->info.args = (struct Parser_ExprVec)gen_dyninit();

    // the exprs at the top act as operands for the new op
    if (Parser_is_ternaryop(op->type))
        gen_dynpush(&op->info.args, out->arr[out->len - 3]);
    if (Parser_is_ternaryop(op->type) || Parser_is_binop(op->type))
        gen_dynpush(&op->info.args, out->arr[out->len - 2]);
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
static void push_operator(const struct Lexer_Token *tok,
                          struct Parser_ExprVec *out,
                          struct Parser_ExprVec *ops, struct DiagVec *diags)
{
    struct Parser_Expr op = op_tok_to_expr(tok);

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

    isize_t i;
    for (i = start; !is_end_type(toks[i].type, end_types, n_end_types); ++i) {
        if (Lexer_is_numlit(toks[i].type))
            gen_dynpush(&out, numlit_tok_to_expr(&toks[i]));
        else if (toks[i].type == LEXER_TOKENTYPE_IDENTIFIER)
            gen_dynpush(&out, ident_tok_to_expr(&toks[i]));
        else if (Lexer_is_op(toks[i].type))
            push_operator(&toks[i], &out, &ops, diags);
        else if (toks[i].type == LEXER_TOKENTYPE_L_PAREN)
            gen_dynpush(&out, parse_subexpr(toks, i, &i, diags));
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
