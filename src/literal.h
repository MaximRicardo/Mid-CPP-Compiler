#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr_type.h"
#include "types.h"
#include <uchar.h>

struct Lit_String {
    TypesCharType *c;
    TypesWCharType *wc;
    char16_t *c16;
    char32_t *c32;

    enum Lit_StringType {
        LIT_STRINGTYPE_CHAR,
        LIT_STRINGTYPE_WCHAR,
        LIT_STRINGTYPE_CHAR16,
        LIT_STRINGTYPE_CHAR32,
    } type;
};

isize_t Lit_strlit_len(const struct Lit_String *strlit);

union Lit_Value {
    // scalars
    i64 sint;
    u64 uint;
    long double flt;

    // strings
    struct Lit_String str;
};
gen_dynarray_struct_named(Lit_ValueVec, union Lit_Value);

void Lit_String_deinit(struct Lit_String *self);

gen_dynarray_struct_named(Lit_StringVec, struct Lit_String);

void Lit_fprint(FILE *out, union Lit_Value val, enum Parser_ExprType type);
void Lit_fprint_toktype(FILE *out, union Lit_Value val,
                        enum Lexer_TokenType type);
void Lit_print(union Lit_Value val, enum Parser_ExprType type);
void Lit_print_toktype(union Lit_Value val, enum Lexer_TokenType type);

struct Lit_ReadIntLitInfo {
    u64 value;
    int base;
} Lit_read_intlit(const char *str, isize_t start, isize_t *out_end);
