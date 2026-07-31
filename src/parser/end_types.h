#pragma once

#include "lexer/token_type.h"
#include "macros.h"

// when parsing an expression or declaration or something like that, you should
// stop at these token types

// most expressions are terminated by a semicolon
constexpr enum MidLexer_TokenType MidParser_default_endtypes[] = {
    MIDLEXER_TOKENTYPE_SEMICOLON};
#define MIDPARSER_DEFAULT_ENDTYPES                                                \
    MidParser_default_endtypes, MID_ARRLEN(MidParser_default_endtypes)

constexpr enum MidLexer_TokenType MidParser_vardecl_endtypes[] = {
    MIDLEXER_TOKENTYPE_SEMICOLON, MIDLEXER_TOKENTYPE_COMMA};
#define MIDPARSER_VARDECL_ENDTYPES                                                \
    MidParser_vardecl_endtypes, MID_ARRLEN(MidParser_vardecl_endtypes)

constexpr enum MidLexer_TokenType MidParser_param_endtypes[] = {
    MIDLEXER_TOKENTYPE_COMMA, MIDLEXER_TOKENTYPE_R_PAREN};
#define MIDPARSER_PARAM_ENDTYPES                                                  \
    MidParser_param_endtypes, MID_ARRLEN(MidParser_param_endtypes)

constexpr enum MidLexer_TokenType MidParser_arg_endtypes[] = {
    MIDLEXER_TOKENTYPE_COMMA, MIDLEXER_TOKENTYPE_R_PAREN};
#define MIDPARSER_ARG_ENDTYPES MidParser_arg_endtypes, MID_ARRLEN(MidParser_arg_endtypes)

constexpr enum MidLexer_TokenType MidParser_tmplt_param_endtypes[] = {
    MIDLEXER_TOKENTYPE_COMMA, MIDLEXER_TTALIAS_R_ANGLE};
#define MIDPARSER_TMPLT_PARAM_ENDTYPES                                            \
    MidParser_tmplt_param_endtypes, MID_ARRLEN(MidParser_tmplt_param_endtypes)

constexpr enum MidLexer_TokenType MidParser_tmplt_arg_endtypes[] = {
    MIDLEXER_TOKENTYPE_COMMA, MIDLEXER_TTALIAS_R_ANGLE};
#define MIDPARSER_TMPLT_ARG_ENDTYPES                                              \
    MidParser_tmplt_arg_endtypes, MID_ARRLEN(MidParser_tmplt_arg_endtypes)
