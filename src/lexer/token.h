#pragma once

#include "generics/dynarray.h"
#include "literal.h"
#include "position.h"

enum Lexer_TokenType {
    LEXER_TOKENTYPE_END,

    LEXER_TOKENTYPE_TERNARYOP_START,
    LEXER_TOKENTYPE_TERNARY_SHUT_COMPILER_UP,
    LEXER_TOKENTYPE_TERNARYOP_END,

    LEXER_TOKENTYPE_BINOP_START,
    LEXER_TOKENTYPE_ADD,
    LEXER_TOKENTYPE_SUB,
    LEXER_TOKENTYPE_MUL,
    LEXER_TOKENTYPE_DIV,
    LEXER_TOKENTYPE_ASSIGN,
    LEXER_TOKENTYPE_BITWISE_AND,
    LEXER_TOKENTYPE_LOGICAL_AND,
    LEXER_TOKENTYPE_COMMA,
    LEXER_TOKENTYPE_BINOP_END,

    LEXER_TOKENTYPE_UNARYOP_START,
    LEXER_TOKENTYPE_DEREF,
    LEXER_TOKENTYPE_REF,
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
    LEXER_TOKENTYPE_L_SQBRACKET,
    LEXER_TOKENTYPE_R_SQBRACKET,

    LEXER_TOKENTYPE_SEMICOLON,

    LEXER_TOKENTYPE_IDENTIFIER,

    LEXER_TOKENTYPE_TYPESPEC_START,
    LEXER_TOKENTYPE_CHAR,
    LEXER_TOKENTYPE_INT,
    LEXER_TOKENTYPE_FLOAT,
    LEXER_TOKENTYPE_DOUBLE,
    LEXER_TOKENTYPE_TYPESPEC_END,

    LEXER_TOKENTYPE_TYPEMOD_START,
    LEXER_TOKENTYPE_SHORT,
    LEXER_TOKENTYPE_LONG,
    LEXER_TOKENTYPE_SIGNED,
    LEXER_TOKENTYPE_UNSIGNED,
    LEXER_TOKENTYPE_TYPEMOD_END,

    // type storage qualifiers
    LEXER_TOKENTYPE_TYPESTORQUAL_START,
    LEXER_TOKENTYPE_STATIC,
    LEXER_TOKENTYPE_CONSTEXPR,
    LEXER_TOKENTYPE_TYPESTORQUAL_END,

    LEXER_TOKENTYPE_TYPEDATAQUAL_START,
    LEXER_TOKENTYPE_CONST,
    LEXER_TOKENTYPE_TYPEDATAQUAL_END,
};

bool Lexer_is_numlit(enum Lexer_TokenType type);
bool Lexer_is_ternaryop(enum Lexer_TokenType type);
bool Lexer_is_binop(enum Lexer_TokenType type);
bool Lexer_is_unaryop(enum Lexer_TokenType type);
bool Lexer_is_op(enum Lexer_TokenType type);
bool Lexer_is_typespec(enum Lexer_TokenType type);
bool Lexer_is_typestorqual(enum Lexer_TokenType type);
bool Lexer_is_typedataqual(enum Lexer_TokenType type);
bool Lexer_is_typequal(enum Lexer_TokenType type);
// signed, unsigned, short and long
bool Lexer_is_typemod(enum Lexer_TokenType type);

struct Lexer_Token {
    union {
        union Literal_Value val;
        const char *ident;
    };
    struct Position pos;
    const char *line;
    enum Lexer_TokenType type;
};
gen_dynarray_struct_named(Lexer_TokenVec, struct Lexer_Token);
