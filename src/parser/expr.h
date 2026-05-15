#pragma once

#include "diag.h"
#include "expr_type.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "parser/type.h"

bool Parser_is_numlit(enum Parser_ExprType type);
bool Parser_is_ternaryop(enum Parser_ExprType type);
bool Parser_is_binop(enum Parser_ExprType type);
bool Parser_is_unaryop(enum Parser_ExprType type);
bool Parser_is_scope_res(enum Parser_ExprType type);
bool Parser_is_op(enum Parser_ExprType type);
bool Parser_is_arith_op(enum Parser_ExprType type);
bool Parser_is_logical_op(enum Parser_ExprType type);
bool Parser_is_comp_op(enum Parser_ExprType type);
// checks for both regular and compound assignment
bool Parser_is_assignment(enum Parser_ExprType type);
bool Parser_is_memb_sel(enum Parser_ExprType type);

// goes from 0 to 15, where 15 is the highest precedence
i32 Parser_op_precedence(enum Parser_ExprType op);
bool Parser_op_ltr_assoc(enum Parser_ExprType op);

bool Parser_expr_uses_args(enum Parser_ExprType type);

enum Parser_ExprValueType {
    PARSER_EXPRVALUE_LVALUE,
    PARSER_EXPRVALUE_PRVALUE,
    PARSER_EXPRVALUE_XVALUE,
};

bool Parser_is_glvalue(enum Parser_ExprValueType type);
bool Parser_is_rvalue(enum Parser_ExprValueType type);

struct Parser_Expr;
gen_dynarray_struct_named(Parser_ExprVec, struct Parser_Expr);

struct Parser_Expr {
    union {
        struct Parser_ExprVec args;
        union Literal_Value val; // NOTE: NON-OWNING! LIFETIME IS MANAGED BY
                                 //       STR_LITS TABLE
        const char *ident;
    } info;

    struct Sema_Scope *res_scope; // used by scope resolutions

    const struct Parser_ASTNode *node; // some expressions may have nodes
                                       // associated with them, like function
                                       // calls referencing the function being
                                       // called
    const struct Lexer_Token *tok;
    struct Parser_Type ret;
    enum Parser_ExprType type;
    enum Parser_ExprValueType valtype;
    bool typechecked; // has this expr been typechecked yet
};

void Parser_Expr_deinit(struct Parser_Expr *expr);
// stops when reaching end_type
struct Parser_Expr Parser_parse_expr(const struct Lexer_Token *toks,
                                     isize_t start,
                                     const enum Lexer_TokenType *end_types,
                                     isize_t n_end_types, isize_t *out_end,
                                     struct Sema_Scope *scope,
                                     struct DiagVec *diags);
// diags - can be NULL if you don't wanna log any errors
isize_t Parser_skip_expr(const struct Lexer_Token *toks, isize_t start,
                         const enum Lexer_TokenType *end_types,
                         isize_t n_end_types, struct DiagVec *diags);
