#pragma once

#include "generics/dynarray.h"
#include "position.h"

enum ErrorType {
    ERRORTYPE_UNKNOWN_SYMBOL,
    ERRORTYPE_MISSING_PAREN,
    ERRORTYPE_MISSING_SQBRACKET,
    ERRORTYPE_MISSING_ANGLE,
    ERRORTYPE_MISSING_CURLY,
    ERRORTYPE_MISSING_IDENTIFIER,
    ERRORTYPE_MISSING_TOKEN,
    ERRORTYPE_MISSING_TYPESPEC,
    ERRORTYPE_MISSING_SEMICOLON,
    ERRORTYPE_MISSING_QUOTE,
    ERRORTYPE_MISSING_COMMA,
    ERRORTYPE_INSUFFICIENT_OPERANDS,
    ERRORTYPE_TYPE_UNSIGNABLE,
    ERRORTYPE_PTR_TO_REF,
    ERRORTYPE_MISPLACED_QUALIFIER,
    ERRORTYPE_TYPE_ALREADY_REF,
    ERRORTYPE_UNEXPECTED_TOKEN,
    ERRORTYPE_TYPEDEF_MISSING_NAME,
    ERRORTYPE_UNDECLARED_IDENTIFIER,
    ERRORTYPE_UNDECLARED_FUNCTION,
    ERRORTYPE_BAD_IDENTIFIER,
    ERRORTYPE_BAD_ARRAY_SUBSCRIPT,
    ERRORTYPE_BAD_ASSIGNMENT,
    ERRORTYPE_BAD_DEREF,
    ERRORTYPE_BAD_REF,
    ERRORTYPE_BAD_CONDITIONAL,
    ERRORTYPE_BAD_ARITHMETIC_OP,
    ERRORTYPE_BAD_LOGICAL_OP,
    ERRORTYPE_BAD_COMPARISON_OP,
    ERRORTYPE_BAD_OP_OVERLOAD,
    ERRORTYPE_BAD_SUPERCLASS,
    ERRORTYPE_BAD_VAR_DECLARATION,
    ERRORTYPE_BAD_DEFAULT_ARGUMENT,
    ERRORTYPE_BAD_LITERAL,
    ERRORTYPE_BAD_MEMB_SEL,
    ERRORTYPE_BAD_RETURN_STMT_TYPE,
    ERRORTYPE_RETURN_OUTSIDE_FUNC,
    ERRORTYPE_BAD_THIS_USAGE,
    ERRORTYPE_NO_MATCHING_CTOR,
    ERRORTYPE_BAD_TYPE,
    ERRORTYPE_BAD_TEMPLATE,
};

enum WarnType {
    WARNTYPE_UNNECESSARY_QUALIFIER,
};

enum DiagType {
    DIAGTYPE_ERROR,
    DIAGTYPE_WARNING,
    DIAGTYPE_NOTE,
};

struct Diag {
    struct Position pos;
    const char *line; // terminated by '\n'
    char *msg;
    union {
        enum ErrorType err;
        enum WarnType warn;
    };
    enum DiagType type;
};
gen_dynarray_struct_named(DiagVec, struct Diag);

void Diag_deinit(struct Diag *self);
void Diag_print(const struct Diag *diag);

struct Lexer_Token;

struct Diag Diag_expected_token_err(const char *name,
                                    const struct Lexer_Token *tok,
                                    enum ErrorType type);
struct Diag Diag_expected_token_warn(const char *name,
                                     const struct Lexer_Token *tok,
                                     enum WarnType type);
struct Diag Diag_unexpected_token_err(const char *name,
                                      const struct Lexer_Token *tok,
                                      enum ErrorType type);
struct Diag Diag_unexpected_token_warn(const char *name,
                                       const struct Lexer_Token *tok,
                                       enum WarnType type);
struct Diag Diag_ident_redefined_err(const char *name,
                                     const struct Lexer_Token *tok,
                                     enum ErrorType type);
struct Diag Diag_ident_redefined_warn(const char *name,
                                      const struct Lexer_Token *tok,
                                      enum WarnType type);
struct Diag Diag_ident_undeclared_err(const char *name,
                                      const struct Lexer_Token *tok,
                                      enum ErrorType type);
struct Diag Diag_ident_undeclared_warn(const char *name,
                                       const struct Lexer_Token *tok,
                                       enum WarnType type);
struct Diag Diag_func_undeclared_err(const char *name,
                                     const struct Lexer_Token *tok,
                                     enum ErrorType type);
struct Diag Diag_func_undeclared_warn(const char *name,
                                      const struct Lexer_Token *tok,
                                      enum WarnType type);
struct Diag Diag_type_id_w_name_err(const struct Lexer_Token *tok,
                                    enum ErrorType type);
struct Diag Diag_type_id_w_name_warn(const struct Lexer_Token *tok,
                                     enum WarnType type);
