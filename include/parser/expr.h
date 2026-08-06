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

bool midpar_is_strlit(enum midpar_ExprType type);
bool midpar_is_fltlit(enum midpar_ExprType type);
bool midpar_is_intlit(enum midpar_ExprType type); // any integral type
bool midpar_is_numlit(enum midpar_ExprType type);
bool midpar_is_ternaryop(enum midpar_ExprType type);
bool midpar_is_binop(enum midpar_ExprType type);
bool midpar_is_unaryop(enum midpar_ExprType type);
bool midpar_is_scope_res(enum midpar_ExprType type);
bool midpar_is_op(enum midpar_ExprType type);
bool midpar_is_arith_op(enum midpar_ExprType type);
bool midpar_is_logical_op(enum midpar_ExprType type);
bool midpar_is_comp_op(enum midpar_ExprType type);
// checks for both regular and compound assignment
bool midpar_is_assignment(enum midpar_ExprType type);
bool midpar_is_memb_sel(enum midpar_ExprType type);

bool midpar_op_has_side_effects(enum midpar_ExprType type);

enum midlit_ValueKind midpar_lit_expr_value_kind(enum midpar_ExprType type);

// goes from 0 to 15, where 15 is the highest precedence
int32_t midpar_op_precedence(enum midpar_ExprType op);
bool midpar_op_ltr_assoc(enum midpar_ExprType op);

bool midpar_expr_uses_args(enum midpar_ExprType type);

enum midpar_ExprValueType {
    MIDPAR_EXPRVALUE_LVALUE,
    MIDPAR_EXPRVALUE_PRVALUE,
    MIDPAR_EXPRVALUE_XVALUE,
};

bool midpar_is_glvalue(enum midpar_ExprValueType type);
bool midpar_is_rvalue(enum midpar_ExprValueType type);

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
struct midpar_Expr midpar_parse_expr(const struct midlex_Token *toks,
                                     mid_isize start,
                                     const enum midlex_TokenType *end_types,
                                     mid_isize n_end_types, mid_isize *out_end,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags);
// diags - can be NULL if you don't wanna log any errors
mid_isize midpar_skip_expr(const struct midlex_Token *toks, mid_isize start,
                           const enum midlex_TokenType *end_types,
                           mid_isize n_end_types, struct mid_DiagVec *diags);

#ifdef __cplusplus
}
#endif
