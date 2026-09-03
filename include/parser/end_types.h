#pragma once

#include "lexer/token_type.h"
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

// when parsing an expression or declaration or something like that, you should
// stop at these token types

// most expressions are terminated by a semicolon
constexpr enum midlex_TokenType midpar_default_endtypes[] = {
    MIDLEX_TOKENTYPE_SEMICOLON};
#define MIDPAR_DEFAULT_ENDTYPES                                                \
    midpar_default_endtypes, MID_ARRLEN(midpar_default_endtypes)

constexpr enum midlex_TokenType midpar_vardecl_endtypes[] = {
    MIDLEX_TOKENTYPE_SEMICOLON, MIDLEX_TOKENTYPE_COMMA};
#define MIDPAR_VARDECL_ENDTYPES                                                \
    midpar_vardecl_endtypes, MID_ARRLEN(midpar_vardecl_endtypes)

constexpr enum midlex_TokenType midpar_param_endtypes[] = {
    MIDLEX_TOKENTYPE_COMMA, MIDLEX_TOKENTYPE_R_PAREN};
#define MIDPAR_PARAM_ENDTYPES                                                  \
    midpar_param_endtypes, MID_ARRLEN(midpar_param_endtypes)

constexpr enum midlex_TokenType midpar_arg_endtypes[] = {
    MIDLEX_TOKENTYPE_COMMA, MIDLEX_TOKENTYPE_R_PAREN};
#define MIDPAR_ARG_ENDTYPES midpar_arg_endtypes, MID_ARRLEN(midpar_arg_endtypes)

constexpr enum midlex_TokenType midpar_tmplt_param_endtypes[] = {
    MIDLEX_TOKENTYPE_COMMA, MIDLEX_TTALIAS_R_ANGLE};
#define MIDPAR_TMPLT_PARAM_ENDTYPES                                            \
    midpar_tmplt_param_endtypes, MID_ARRLEN(midpar_tmplt_param_endtypes)

constexpr enum midlex_TokenType midpar_tmplt_arg_endtypes[] = {
    MIDLEX_TOKENTYPE_COMMA, MIDLEX_TTALIAS_R_ANGLE};
#define MIDPAR_TMPLT_ARG_ENDTYPES                                              \
    midpar_tmplt_arg_endtypes, MID_ARRLEN(midpar_tmplt_arg_endtypes)

constexpr enum midlex_TokenType midpar_subscript_endtypes[] = {
    MIDLEX_TOKENTYPE_R_SQBRACKET};
#define MIDPAR_SUBSCRIPT_ENDTYPES                                              \
    midpar_subscript_endtypes, MID_ARRLEN(midpar_subscript_endtypes)

#ifdef __cplusplus
}
#endif
