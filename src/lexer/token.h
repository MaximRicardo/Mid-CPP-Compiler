#pragma once

#include "generics/dynarray.h"
#include "literal.h"
#include "position.h"

enum Lexer_TokenType {
    LEXER_TOKENTYPE_TERNARYOP_START,
    LEXER_TOKENTYPE_TERNARY_SHUT_COMPILER_UP,
    LEXER_TOKENTYPE_TERNARYOP_END,

    LEXER_TOKENTYPE_BINOP_START,
    LEXER_TOKENTYPE_ADD,
    LEXER_TOKENTYPE_SUB,
    LEXER_TOKENTYPE_MUL,
    LEXER_TOKENTYPE_DIV,
    LEXER_TOKENTYPE_BINOP_END,

    LEXER_TOKENTYPE_UNARYOP_START,
    LEXER_TOKENTYPE_UNARY_SHUT_COMPILER_UP,
    LEXER_TOKENTYPE_UNARYOP_END,

    LEXER_TOKENTYPE_NUMLIT_START,
    LEXER_TOKENTYPE_INT_LIT,
    LEXER_TOKENTYPE_UINT_LIT,
    LEXER_TOKENTYPE_LONG_LIT,
    LEXER_TOKENTYPE_ULONG_LIT,
    LEXER_TOKENTYPE_LONGLONG_LIT,
    LEXER_TOKENTYPE_ULONGLONG_LIT,
    LEXER_TOKENTYPE_FLOAT_LIT,
    LEXER_TOKENTYPE_DOUBLE_LIT,
    LEXER_TOKENTYPE_LONGDOUBLE_LIT,
    LEXER_TOKENTYPE_NUMLIT_END,

    LEXER_TOKENTYPE_L_PAREN,
    LEXER_TOKENTYPE_R_PAREN,
};

bool Lexer_is_numlit(enum Lexer_TokenType type);
bool Lexer_is_ternaryop(enum Lexer_TokenType type);
bool Lexer_is_binop(enum Lexer_TokenType type);
bool Lexer_is_unaryop(enum Lexer_TokenType type);
bool Lexer_is_op(enum Lexer_TokenType type);

struct Lexer_Token {
    union Literal_Value val;
    struct Position pos;
    const char *line;
    enum Lexer_TokenType type;
};
gen_dynarray_struct_named(Lexer_TokenVec, struct Lexer_Token);
