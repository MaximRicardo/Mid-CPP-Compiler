#pragma once

#include "generics/dynarray.h"
#include "literal.h"
#include "position.h"
#include "token_type.h"

bool midlex_is_numlit(enum midlex_TokenType type);
bool midlex_is_strlit(enum midlex_TokenType type);
bool midlex_is_lit(enum midlex_TokenType type);
bool midlex_is_ternaryop(enum midlex_TokenType type);
bool midlex_is_binop(enum midlex_TokenType type);
bool midlex_is_unaryop(enum midlex_TokenType type);
// operators that can be unary (eg. MIDLEX_TOKENTYPE_SUB can be in a - b and
// -b)
bool midlex_can_be_unary(enum midlex_TokenType type);
bool midlex_is_op(enum midlex_TokenType type);
bool midlex_is_typespec(enum midlex_TokenType type);
// class, struct, union and enum
bool midlex_is_named_typespec(enum midlex_TokenType type);
bool midlex_is_typestorqual(enum midlex_TokenType type);
bool midlex_is_typedataqual(enum midlex_TokenType type);
bool midlex_is_typequal(enum midlex_TokenType type);
// signed, unsigned, short and long
bool midlex_is_typemod(enum midlex_TokenType type);
bool midlex_is_accessspec(enum midlex_TokenType type);

struct midlex_Token {
    union {
        union midlit_Value val; // NOTE: NON-OWNING! LIFE-TIME IS MANAGED BY
                                //       THE STR_LITS TABLE
        const char *ident;
    };
    struct mid_Position pos;
    const char *line;
    enum midlex_TokenType type;
};
midgen_dynarray_struct_named(midlex_TokenVec, struct midlex_Token);
