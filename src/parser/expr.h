#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"

enum Parser_ExprType {
    // num literals
    PARSER_EXPRTYPE_NUMLIT_START,
    PARSER_EXPRTYPE_INT_LIT,
    PARSER_EXPRTYPE_UINT_LIT,
    PARSER_EXPRTYPE_LONG_LIT,
    PARSER_EXPRTYPE_ULONG_LIT,
    PARSER_EXPRTYPE_LONGLONG_LIT,
    PARSER_EXPRTYPE_ULONGLONG_LIT,
    PARSER_EXPRTYPE_FLOAT_LIT,
    PARSER_EXPRTYPE_DOUBLE_LIT,
    PARSER_EXPRTYPE_LONGDOUBLE_LIT,
    PARSER_EXPRTYPE_NUMLIT_END,

    PARSER_EXPRTYPE_IDENTIFIER,

    // ternary ops
    PARSER_EXPRTYPE_TERNARYOP_START,
    PARSER_EXPRTYPE_TERNARY_SHUT_COMPILER_UP,
    PARSER_EXPRTYPE_TERNARYOP_END,

    // binary ops
    PARSER_EXPRTYPE_BINOP_START,
    PARSER_EXPRTYPE_ADD,
    PARSER_EXPRTYPE_SUB,
    PARSER_EXPRTYPE_MUL,
    PARSER_EXPRTYPE_DIV,
    PARSER_EXPRTYPE_ASSIGN,
    PARSER_EXPRTYPE_BITWISE_AND,
    PARSER_EXPRTYPE_LOGICAL_AND,
    PARSER_EXPRTYPE_COMMA,
    PARSER_EXPRTYPE_BINOP_END,

    // unary ops
    PARSER_EXPRTYPE_UNARYOP_START,
    PARSER_EXPRTYPE_DEREF,
    PARSER_EXPRTYPE_REF,
    PARSER_EXPRTYPE_UNARYOP_END,
};

bool Parser_is_numlit(enum Parser_ExprType type);
bool Parser_is_ternaryop(enum Parser_ExprType type);
bool Parser_is_binop(enum Parser_ExprType type);
bool Parser_is_unaryop(enum Parser_ExprType type);
bool Parser_is_op(enum Parser_ExprType type);

// goes from 0 to 15, where 15 is the highest precedence
i32 Parser_op_precedence(enum Parser_ExprType op);
bool Parser_op_ltr_assoc(enum Parser_ExprType op);

bool Parser_expr_uses_args(enum Parser_ExprType type);

struct Parser_Expr;
gen_dynarray_struct_named(Parser_ExprVec, struct Parser_Expr);

struct Parser_Expr {
    union {
        struct Parser_ExprVec args;
        union Literal_Value val;
        const char *ident;
    } info;

    const struct Lexer_Token *tok;
    enum Parser_ExprType type;
};

void Parser_Expr_deinit(struct Parser_Expr *expr);
// stops when reaching end_type
struct Parser_Expr Parser_parse_expr(const struct Lexer_Token *toks,
                                     isize_t start,
                                     const enum Lexer_TokenType *end_types,
                                     isize_t n_end_types, isize_t *out_end,
                                     struct DiagVec *diags);
// evaluate the result of a constant expression
union Literal_Value Parser_evaluate(const struct Parser_Expr *expr);
