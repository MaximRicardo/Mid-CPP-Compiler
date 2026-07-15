#pragma once

#include "lexer/token_type.h"
#include "macros.h"

// when parsing an expression or declaration or something like that, you should
// stop at these token types

// most expressions are terminated by a semicolon
constexpr enum Lexer_TokenType Parser_default_endtypes[] = {
    LEXER_TOKENTYPE_SEMICOLON};
#define PARSER_DEFAULT_ENDTYPES                                                \
    Parser_default_endtypes, ARRLEN(Parser_default_endtypes)

constexpr enum Lexer_TokenType Parser_vardecl_endtypes[] = {
    LEXER_TOKENTYPE_SEMICOLON, LEXER_TOKENTYPE_COMMA};
#define PARSER_VARDECL_ENDTYPES                                                \
    Parser_vardecl_endtypes, ARRLEN(Parser_vardecl_endtypes)

constexpr enum Lexer_TokenType Parser_param_endtypes[] = {
    LEXER_TOKENTYPE_COMMA, LEXER_TOKENTYPE_R_PAREN};
#define PARSER_PARAM_ENDTYPES                                                  \
    Parser_param_endtypes, ARRLEN(Parser_param_endtypes)

constexpr enum Lexer_TokenType Parser_arg_endtypes[] = {
    LEXER_TOKENTYPE_COMMA, LEXER_TOKENTYPE_R_PAREN};
#define PARSER_ARG_ENDTYPES Parser_arg_endtypes, ARRLEN(Parser_arg_endtypes)

constexpr enum Lexer_TokenType Parser_tmplt_param_endtypes[] = {
    LEXER_TOKENTYPE_COMMA, LEXER_TTALIAS_R_ANGLE};
#define PARSER_TMPLT_PARAM_ENDTYPES                                            \
    Parser_tmplt_param_endtypes, ARRLEN(Parser_tmplt_param_endtypes)

constexpr enum Lexer_TokenType Parser_tmplt_arg_endtypes[] = {
    LEXER_TOKENTYPE_COMMA, LEXER_TTALIAS_R_ANGLE};
#define PARSER_TMPLT_ARG_ENDTYPES                                              \
    Parser_tmplt_arg_endtypes, ARRLEN(Parser_tmplt_arg_endtypes)
