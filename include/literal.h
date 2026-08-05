#pragma once

#include "apfloat.h"
#include "apint.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr_type.h"
#include "types.h"
#include <uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

enum midlit_StringType {
    MIDLIT_STRINGTYPE_CHAR,
    MIDLIT_STRINGTYPE_WCHAR,
    MIDLIT_STRINGTYPE_CHAR16,
    MIDLIT_STRINGTYPE_CHAR32,
};

// in bytes
int midlit_strtype_char_size(enum midlit_StringType type);

struct midlit_String {
    TypesCharType *c;
    TypesWCharType *wc;
    char16_t *c16;
    char32_t *c32;

    enum midlit_StringType type;
};
midgen_dynarray_struct_named(midlit_StringVec, struct midlit_String);

void midlit_String_deinit(struct midlit_String *self);
mid_isize midlit_strlit_len(const struct midlit_String *strlit);
void midlit_fprint_strlit(FILE *out, const struct midlit_String *self);
void midlit_print_strlit(const struct midlit_String *self);

enum midlit_ValueKind {
    MIDLIT_VALUE_SIGNED_INT,
    MIDLIT_VALUE_UNSIGNED_INT,
    MIDLIT_VALUE_FLOAT,
    MIDLIT_VALUE_STR,
};

union midlit_Value {
    // scalars
    struct mid_APInt i;
    struct mid_APFloat flt;

    // strings
    struct midlit_String str;
};
midgen_dynarray_struct_named(midlit_ValueVec, union midlit_Value);

void midlit_Value_deinit(union midlit_Value *self, enum midlit_ValueKind kind);

struct midlit_TaggedValue {
    union midlit_Value v;
    enum midlit_ValueKind kind;
};

void midlit_TaggedValue_deinit(struct midlit_TaggedValue *self);
struct midlit_TaggedValue
midlit_copy_value(const struct midlit_TaggedValue *src);

void midlit_tagged_fprint(FILE *out, const struct midlit_TaggedValue *val);
void midlit_tagged_print(const struct midlit_TaggedValue *val);
void midlit_fprint(FILE *out, const union midlit_Value *val,
                   enum midpar_ExprType type);
void midlit_fprint_toktype(FILE *out, const union midlit_Value *val,
                           enum midlex_TokenType type);
void midlit_print(const union midlit_Value *val, enum midpar_ExprType type);
void midlit_print_toktype(const union midlit_Value *val,
                          enum midlex_TokenType type);

struct midlit_ReadIntLitInfo {
    struct mid_APInt value;
    int base;
};
// TODO: add support for APInt
struct midlit_ReadIntLitInfo
midlit_read_intlit(const char *str, mid_isize start, mid_isize *out_end);

#ifdef __cplusplus
}
#endif
