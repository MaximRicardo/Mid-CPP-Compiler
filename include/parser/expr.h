#pragma once

#include "diag.h"
#include "expr_type.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "parser/type.h"

#ifdef __cplusplus
extern "C" {
#endif

enum midpar_ExprValueType {
    MIDPAR_EXPRVALUE_LVALUE,
    MIDPAR_EXPRVALUE_PRVALUE,
    MIDPAR_EXPRVALUE_XVALUE,
};

struct midpar_Expr;
midgen_dynarray_struct_named(midpar_ExprVec, struct midpar_Expr);

struct midpar_Expr {
    union {
        struct midpar_ExprVec args;
        struct midlit_TaggedValue
            val; // NOTE: STR LITS ARE NON-OWNING! LIFETIME IS MANAGED BY
                 //       STR_LITS TABLE
        const char *ident;
    } info;

    struct midsema_Scope *res_scope; // used by scope resolutions

    struct midpar_ASTNode *node; // some expressions may have nodes
                                 // associated with them:
                                 //
                                 // function calls reference the func
                                 // being called.
                                 //
                                 // overloaded operators reference the
                                 // function holding the overload.
    const struct midlex_Token *tok;
    struct midpar_Type ret;
    enum midpar_ExprType type;
    enum midpar_ExprValueType valtype;
    bool overloaded;  // did this expression get overloaded by an operator
                      // overload
    bool typechecked; // has this expr been typechecked yet
    bool constant;    // can be evaluated at compile time
};

void midpar_Expr_deinit(struct midpar_Expr *expr);
struct midpar_Expr midpar_copy_expr(const struct midpar_Expr *expr);
// stops when reaching end_type
struct midpar_Expr midpar_parse_expr(midlex_TokenIter start,
                                     const enum midlex_TokenType *end_types,
                                     mid_isize n_end_types,
                                     midlex_TokenIter *out_end,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags);
// diags - can be NULL if you don't wanna log any errors
midlex_TokenIter midpar_skip_expr(midlex_TokenIter start,
                                  const enum midlex_TokenType *end_types,
                                  mid_isize n_end_types,
                                  struct mid_DiagVec *diags);

#ifdef __cplusplus
}
#endif
