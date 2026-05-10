#pragma once

#include "generics/dynarray.h"
#include "literal.h"
#include "position.h"
#include "token_type.h"

bool Lexer_is_numlit(enum Lexer_TokenType type);
bool Lexer_is_strlit(enum Lexer_TokenType type);
bool Lexer_is_lit(enum Lexer_TokenType type);
bool Lexer_is_ternaryop(enum Lexer_TokenType type);
bool Lexer_is_binop(enum Lexer_TokenType type);
bool Lexer_is_unaryop(enum Lexer_TokenType type);
// operators that can be unary (eg. LEXER_TOKENTYPE_SUB can be in a - b and -b)
bool Lexer_can_be_unary(enum Lexer_TokenType type);
bool Lexer_is_op(enum Lexer_TokenType type);
bool Lexer_is_typespec(enum Lexer_TokenType type);
// class, struct, union and enum
bool Lexer_is_named_typespec(enum Lexer_TokenType type);
bool Lexer_is_typestorqual(enum Lexer_TokenType type);
bool Lexer_is_typedataqual(enum Lexer_TokenType type);
bool Lexer_is_typequal(enum Lexer_TokenType type);
// signed, unsigned, short and long
bool Lexer_is_typemod(enum Lexer_TokenType type);
bool Lexer_is_accessspec(enum Lexer_TokenType type);

struct Lexer_Token {
    union {
        union Literal_Value val; // NOTE: NON-OWNING! LIFE-TIME IS MANAGED BY
                                 //       THE STR_LITS TABLE
        const char *ident;
    };
    struct Position pos;
    const char *line;
    enum Lexer_TokenType type;
};
gen_dynarray_struct_named(Lexer_TokenVec, struct Lexer_Token);
