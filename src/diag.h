#pragma once

#include "generics/dynarray.h"
#include "position.h"

enum MidDiag_ErrT {
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
};

enum MidDiag_WarnT {
    MIDDIAG_WARN_UNNECESSARY_QUALIFIER,
};

enum MidDiag_Type {
    MIDDIAG_TYPE_ERROR,
    MIDDIAG_TYPE_WARNING,
    MIDDIAG_TYPE_NOTE,
};

struct MidDiag_Diag {
    struct Mid_Position pos;
    const char *line; // terminated by '\n'
    char *msg;
    union {
        enum MidDiag_ErrT err;
        enum MidDiag_WarnT warn;
    };
    enum MidDiag_Type type;
};
MidGen_dynarray_struct_named(MidDiag_DiagVec, struct MidDiag_Diag);

void MidDiag_deinit(struct MidDiag_Diag *self);
void MidDiag_print(const struct MidDiag_Diag *diag);

struct MidLexer_Token;

struct MidDiag_Diag MidDiag_expected_token_err(const char *name,
                                    const struct MidLexer_Token *tok,
                                    enum MidDiag_ErrT type);
struct MidDiag_Diag MidDiag_expected_token_warn(const char *name,
                                     const struct MidLexer_Token *tok,
                                     enum MidDiag_WarnT type);
struct MidDiag_Diag MidDiag_unexpected_token_err(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_ErrT type);
struct MidDiag_Diag MidDiag_unexpected_token_warn(const char *name,
                                       const struct MidLexer_Token *tok,
                                       enum MidDiag_WarnT type);
struct MidDiag_Diag MidDiag_ident_redefined_err(const char *name,
                                     const struct MidLexer_Token *tok,
                                     enum MidDiag_ErrT type);
struct MidDiag_Diag MidDiag_ident_redefined_warn(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_WarnT type);
struct MidDiag_Diag MidDiag_ident_undeclared_err(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_ErrT type);
struct MidDiag_Diag MidDiag_ident_undeclared_warn(const char *name,
                                       const struct MidLexer_Token *tok,
                                       enum MidDiag_WarnT type);
struct MidDiag_Diag MidDiag_func_undeclared_err(const char *name,
                                     const struct MidLexer_Token *tok,
                                     enum MidDiag_ErrT type);
struct MidDiag_Diag MidDiag_func_undeclared_warn(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_WarnT type);
struct MidDiag_Diag MidDiag_type_id_w_name_err(const struct MidLexer_Token *tok,
                                    enum MidDiag_ErrT type);
struct MidDiag_Diag MidDiag_type_id_w_name_warn(const struct MidLexer_Token *tok,
                                     enum MidDiag_WarnT type);
