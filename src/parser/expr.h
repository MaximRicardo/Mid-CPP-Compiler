#pragma once

#include "diag.h"
#include "expr_type.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "parser/type.h"

bool MidParser_is_strlit(enum MidParser_ExprType type);
bool MidParser_is_numlit(enum MidParser_ExprType type);
bool MidParser_is_ternaryop(enum MidParser_ExprType type);
bool MidParser_is_binop(enum MidParser_ExprType type);
bool MidParser_is_unaryop(enum MidParser_ExprType type);
bool MidParser_is_scope_res(enum MidParser_ExprType type);
bool MidParser_is_op(enum MidParser_ExprType type);
bool MidParser_is_arith_op(enum MidParser_ExprType type);
bool MidParser_is_logical_op(enum MidParser_ExprType type);
bool MidParser_is_comp_op(enum MidParser_ExprType type);
// checks for both regular and compound assignment
bool MidParser_is_assignment(enum MidParser_ExprType type);
bool MidParser_is_memb_sel(enum MidParser_ExprType type);

// goes from 0 to 15, where 15 is the highest precedence
i32 MidParser_op_precedence(enum MidParser_ExprType op);
bool MidParser_op_ltr_assoc(enum MidParser_ExprType op);

bool MidParser_expr_uses_args(enum MidParser_ExprType type);

enum MidParser_ExprValueType {
    MIDPARSER_EXPRVALUE_LVALUE,
    MIDPARSER_EXPRVALUE_PRVALUE,
    MIDPARSER_EXPRVALUE_XVALUE,
};

bool MidParser_is_glvalue(enum MidParser_ExprValueType type);
bool MidParser_is_rvalue(enum MidParser_ExprValueType type);

struct MidParser_Expr;
MidGen_dynarray_struct_named(MidParser_ExprVec, struct MidParser_Expr);

struct MidParser_Expr {
    union {
        struct MidParser_ExprVec args;
        union MidLit_Value val; // NOTE: NON-OWNING! LIFETIME IS MANAGED BY
                             //       STR_LITS TABLE
        const char *ident;
    } info;

    struct MidSema_Scope *res_scope; // used by scope resolutions

    struct MidParser_ASTNode *node; // some expressions may have nodes
                                 // associated with them:
                                 //
                                 // function calls reference the func
                                 // being called.
                                 //
                                 // overloaded operators reference the
                                 // function holding the overload.
    const struct MidLexer_Token *tok;
    struct MidParser_Type ret;
    enum MidParser_ExprType type;
    enum MidParser_ExprValueType valtype;
    bool overloaded;  // did this expression get overloaded by an operator
                      // overload
    bool typechecked; // has this expr been typechecked yet
};

void MidParser_Expr_deinit(struct MidParser_Expr *expr);
struct MidParser_Expr MidParser_copy_expr(const struct MidParser_Expr *expr);
// stops when reaching end_type
struct MidParser_Expr MidParser_parse_expr(const struct MidLexer_Token *toks,
                                     mid_isize start,
                                     const enum MidLexer_TokenType *end_types,
                                     mid_isize n_end_types, mid_isize *out_end,
                                     struct MidSema_Scope *scope,
                                     struct MidDiag_DiagVec *diags);
// diags - can be NULL if you don't wanna log any errors
mid_isize MidParser_skip_expr(const struct MidLexer_Token *toks, mid_isize start,
                         const enum MidLexer_TokenType *end_types,
                         mid_isize n_end_types, struct MidDiag_DiagVec *diags);
