#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr_type.h"
#include "types.h"
#include <uchar.h>

struct MidLit_String {
    TypesCharType *c;
    TypesWCharType *wc;
    char16_t *c16;
    char32_t *c32;

    enum MidLit_StringType {
        MIDLIT_STRINGTYPE_CHAR,
        MIDLIT_STRINGTYPE_WCHAR,
        MIDLIT_STRINGTYPE_CHAR16,
        MIDLIT_STRINGTYPE_CHAR32,
    } type;
};

mid_isize MidLit_strlit_len(const struct MidLit_String *strlit);

union MidLit_Value {
    // scalars
    i64 sint;
    u64 uint;
    long double flt;

    // strings
    struct MidLit_String str;
};
MidGen_dynarray_struct_named(MidLit_ValueVec, union MidLit_Value);

void MidLit_String_deinit(struct MidLit_String *self);

MidGen_dynarray_struct_named(MidLit_StringVec, struct MidLit_String);

void MidLit_fprint(FILE *out, union MidLit_Value val,
                   enum MidParser_ExprType type);
void MidLit_fprint_toktype(FILE *out, union MidLit_Value val,
                           enum MidLexer_TokenType type);
void MidLit_print(union MidLit_Value val, enum MidParser_ExprType type);
void MidLit_print_toktype(union MidLit_Value val, enum MidLexer_TokenType type);

struct MidLit_ReadIntLitInfo {
    u64 value;
    int base;
} MidLit_read_intlit(const char *str, mid_isize start, mid_isize *out_end);
