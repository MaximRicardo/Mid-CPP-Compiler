#pragma once

#include "generics/dynarray.h"
#include "literal.h"
#include "position.h"
#include "token_type.h"

bool MidLexer_is_numlit(enum MidLexer_TokenType type);
bool MidLexer_is_strlit(enum MidLexer_TokenType type);
bool MidLexer_is_lit(enum MidLexer_TokenType type);
bool MidLexer_is_ternaryop(enum MidLexer_TokenType type);
bool MidLexer_is_binop(enum MidLexer_TokenType type);
bool MidLexer_is_unaryop(enum MidLexer_TokenType type);
// operators that can be unary (eg. MIDLEXER_TOKENTYPE_SUB can be in a - b and
// -b)
bool MidLexer_can_be_unary(enum MidLexer_TokenType type);
bool MidLexer_is_op(enum MidLexer_TokenType type);
bool MidLexer_is_typespec(enum MidLexer_TokenType type);
// class, struct, union and enum
bool MidLexer_is_named_typespec(enum MidLexer_TokenType type);
bool MidLexer_is_typestorqual(enum MidLexer_TokenType type);
bool MidLexer_is_typedataqual(enum MidLexer_TokenType type);
bool MidLexer_is_typequal(enum MidLexer_TokenType type);
// signed, unsigned, short and long
bool MidLexer_is_typemod(enum MidLexer_TokenType type);
bool MidLexer_is_accessspec(enum MidLexer_TokenType type);

struct MidLexer_Token {
    union {
        union MidLit_Value val; // NOTE: NON-OWNING! LIFE-TIME IS MANAGED BY
                                //       THE STR_LITS TABLE
        const char *ident;
    };
    struct Mid_Position pos;
    const char *line;
    enum MidLexer_TokenType type;
};
MidGen_dynarray_struct_named(MidLexer_TokenVec, struct MidLexer_Token);
