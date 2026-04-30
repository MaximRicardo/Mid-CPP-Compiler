#pragma once

#include "lexer/token.h"
#include "macros.h"

// when parsing an expression or declaration or something like that, you should
// stop at these token types

// most expressions are terminated by a semicolon
constexpr enum Lexer_TokenType Parser_default_endtypes[] = {
    LEXER_TOKENTYPE_SEMICOLON};
#define PARSER_DEFAULT_ENDTYPES                                                \
    Parser_default_endtypes, ARRLEN(Parser_default_endtypes)

constexpr enum Lexer_TokenType Parser_param_endtypes[] = {
    LEXER_TOKENTYPE_COMMA, LEXER_TOKENTYPE_R_PAREN};
#define PARSER_PARAM_ENDTYPES                                                  \
    Parser_param_endtypes, ARRLEN(Parser_param_endtypes)
