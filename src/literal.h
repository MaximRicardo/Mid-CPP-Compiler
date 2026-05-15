#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr_type.h"
#include "types.h"
#include <uchar.h>

struct Literal_String {
    TypesCharType *c;
    TypesWCharType *wc;
    char16_t *c16;
    char32_t *c32;

    enum Literal_StringType {
        LITERAL_STRINGTYPE_CHAR,
        LITERAL_STRINGTYPE_WCHAR,
        LITERAL_STRINGTYPE_CHAR16,
        LITERAL_STRINGTYPE_CHAR32,
    } type;
};

isize_t Literal_strlit_len(const struct Literal_String *strlit);

union Literal_Value {
    // scalars
    i64 sint;
    u64 uint;
    long double flt;
    void *ptr;

    // strings
    struct Literal_String str;
};
gen_dynarray_struct_named(Literal_ValueVec, union Literal_Value);

void Literal_String_deinit(struct Literal_String *self);

gen_dynarray_struct_named(Literal_StringVec, struct Literal_String);

void Literal_fprint(FILE *out, union Literal_Value val,
                    enum Parser_ExprType type);
void Literal_fprint_toktype(FILE *out, union Literal_Value val,
                            enum Lexer_TokenType type);
void Literal_print(union Literal_Value val, enum Parser_ExprType type);
void Literal_print_toktype(union Literal_Value val, enum Lexer_TokenType type);
