#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr_type.h"
#include "types.h"
#include <uchar.h>

struct midlit_String {
    TypesCharType *c;
    TypesWCharType *wc;
    char16_t *c16;
    char32_t *c32;

    enum midlit_StringType {
        MIDLIT_STRINGTYPE_CHAR,
        MIDLIT_STRINGTYPE_WCHAR,
        MIDLIT_STRINGTYPE_CHAR16,
        MIDLIT_STRINGTYPE_CHAR32,
    } type;
};

mid_isize midlit_strlit_len(const struct midlit_String *strlit);

union midlit_Value {
    // scalars
    i64 sint;
    u64 uint;
    long double flt;

    // strings
    struct midlit_String str;
};
midgen_dynarray_struct_named(midlit_ValueVec, union midlit_Value);

void midlit_String_deinit(struct midlit_String *self);

midgen_dynarray_struct_named(midlit_StringVec, struct midlit_String);

void midlit_fprint(FILE *out, union midlit_Value val,
                   enum midpar_ExprType type);
void midlit_fprint_toktype(FILE *out, union midlit_Value val,
                           enum midlex_TokenType type);
void midlit_print(union midlit_Value val, enum midpar_ExprType type);
void midlit_print_toktype(union midlit_Value val, enum midlex_TokenType type);

struct midlit_ReadIntLitInfo {
    u64 value;
    int base;
} midlit_read_intlit(const char *str, mid_isize start, mid_isize *out_end);
