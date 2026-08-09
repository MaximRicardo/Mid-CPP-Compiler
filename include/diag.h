#pragma once

#include "generics/dynarray.h"
#include "position.h"

#ifdef __cplusplus
extern "C" {
#endif

enum middiag_ErrT {
    MIDDIAG_ERR_UNKNOWN_SYMBOL,
    MIDDIAG_ERR_MISSING_PAREN,
    MIDDIAG_ERR_MISSING_SQBRACKET,
    MIDDIAG_ERR_MISSING_ANGLE,
    MIDDIAG_ERR_MISSING_CURLY,
    MIDDIAG_ERR_MISSING_IDENTIFIER,
    MIDDIAG_ERR_MISSING_TOKEN,
    MIDDIAG_ERR_MISSING_TYPESPEC,
    MIDDIAG_ERR_MISSING_SEMICOLON,
    MIDDIAG_ERR_MISSING_QUOTE,
    MIDDIAG_ERR_MISSING_COMMA,
    MIDDIAG_ERR_INSUFFICIENT_OPERANDS,
    MIDDIAG_ERR_TYPE_UNSIGNABLE,
    MIDDIAG_ERR_PTR_TO_REF,
    MIDDIAG_ERR_MISPLACED_QUALIFIER,
    MIDDIAG_ERR_TYPE_ALREADY_REF,
    MIDDIAG_ERR_UNEXPECTED_TOKEN,
    MIDDIAG_ERR_TYPEDEF_MISSING_NAME,
    MIDDIAG_ERR_UNDECLARED_IDENTIFIER,
    MIDDIAG_ERR_UNDECLARED_FUNCTION,
    MIDDIAG_ERR_BAD_IDENTIFIER,
    MIDDIAG_ERR_BAD_ARRAY_SUBSCRIPT,
    MIDDIAG_ERR_BAD_ASSIGNMENT,
    MIDDIAG_ERR_BAD_DEREF,
    MIDDIAG_ERR_BAD_REF,
    MIDDIAG_ERR_BAD_CONDITIONAL,
    MIDDIAG_ERR_BAD_ARITHMETIC_OP,
    MIDDIAG_ERR_BAD_LOGICAL_OP,
    MIDDIAG_ERR_BAD_COMPARISON_OP,
    MIDDIAG_ERR_BAD_OP_OVERLOAD,
    MIDDIAG_ERR_BAD_SUPERCLASS,
    MIDDIAG_ERR_BAD_VAR_DECLARATION,
    MIDDIAG_ERR_BAD_DEFAULT_ARGUMENT,
    MIDDIAG_ERR_BAD_LITERAL,
    MIDDIAG_ERR_BAD_MEMB_SEL,
    MIDDIAG_ERR_BAD_RETURN_STMT_TYPE,
    MIDDIAG_ERR_RETURN_OUTSIDE_FUNC,
    MIDDIAG_ERR_BAD_THIS_USAGE,
    MIDDIAG_ERR_NO_MATCHING_CTOR,
    MIDDIAG_ERR_BAD_TYPE,
    MIDDIAG_ERR_BAD_TEMPLATE,
    MIDDIAG_ERR_BAD_CONSTEXPR,
    MIDDIAG_ERR_BAD_CTOR_MEMB_INIT_LIST,
};

enum middiag_WarnT {
    MIDDIAG_WARN_UNNECESSARY_QUALIFIER,
};

enum middiag_Type {
    MIDDIAG_TYPE_ERROR,
    MIDDIAG_TYPE_WARNING,
    MIDDIAG_TYPE_NOTE,
};

struct mid_Diag {
    struct mid_Position pos;
    const char *line; // terminated by '\n'
    char *msg;
    union {
        enum middiag_ErrT err;
        enum middiag_WarnT warn;
    };
    enum middiag_Type type;
};
midgen_dynarray_struct_named(mid_DiagVec, struct mid_Diag);

void middiag_deinit(struct mid_Diag *self);
void middiag_print(const struct mid_Diag *diag);

struct midlex_Token;

struct mid_Diag middiag_expected_token_err(const char *name,
                                           const struct midlex_Token *tok,
                                           enum middiag_ErrT type);
struct mid_Diag middiag_expected_token_warn(const char *name,
                                            const struct midlex_Token *tok,
                                            enum middiag_WarnT type);
struct mid_Diag middiag_unexpected_token_err(const char *name,
                                             const struct midlex_Token *tok,
                                             enum middiag_ErrT type);
struct mid_Diag middiag_unexpected_token_warn(const char *name,
                                              const struct midlex_Token *tok,
                                              enum middiag_WarnT type);
struct mid_Diag middiag_ident_redefined_err(const char *name,
                                            const struct midlex_Token *tok,
                                            enum middiag_ErrT type);
struct mid_Diag middiag_ident_redefined_warn(const char *name,
                                             const struct midlex_Token *tok,
                                             enum middiag_WarnT type);
struct mid_Diag middiag_ident_undeclared_err(const char *name,
                                             const struct midlex_Token *tok,
                                             enum middiag_ErrT type);
struct mid_Diag middiag_ident_undeclared_warn(const char *name,
                                              const struct midlex_Token *tok,
                                              enum middiag_WarnT type);
struct mid_Diag middiag_func_undeclared_err(const char *name,
                                            const struct midlex_Token *tok,
                                            enum middiag_ErrT type);
struct mid_Diag middiag_func_undeclared_warn(const char *name,
                                             const struct midlex_Token *tok,
                                             enum middiag_WarnT type);
struct mid_Diag middiag_type_id_w_name_err(const struct midlex_Token *tok,
                                           enum middiag_ErrT type);
struct mid_Diag middiag_type_id_w_name_warn(const struct midlex_Token *tok,
                                            enum middiag_WarnT type);

#ifdef __cplusplus
}
#endif
