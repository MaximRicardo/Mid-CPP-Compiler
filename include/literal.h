#pragma once

// TODO: move this into the sema module

#include "apfloat.h"
#include "apint.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr_type.h"
#include "sema/class_lit.h"
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

    // the string stored as arbitrary precision integers.
    // includes the null terminator
    struct midlit_TaggedValue *nums;

    // doesn't include the null terminator
    uint_least64_t len;

    enum midlit_StringType type;
};
midgen_dynarray_struct_named(midlit_StringVec, struct midlit_String);

void midlit_String_deinit(struct midlit_String *self);
void midlit_setup_string_nums(struct midlit_String *self);
mid_isize midlit_strlit_len(const struct midlit_String *strlit);
void midlit_fprint_strlit(FILE *out, const struct midlit_String *self);
void midlit_print_strlit(const struct midlit_String *self);

struct midlit_Array {
    struct midlit_TaggedValue *elems;
    uint_least64_t len;
};

void midlit_Array_deinit(struct midlit_Array *self);
struct midlit_Array midlit_copy_array(const struct midlit_Array *src);
void midlit_fprint_array(FILE *out, const struct midlit_Array *self);
void midlit_print_array(const struct midlit_Array *self);

enum midlit_ValueKind {
    MIDLIT_VALUE_NONE,
    MIDLIT_VALUE_SIGNED_INT,
    MIDLIT_VALUE_UNSIGNED_INT,
    MIDLIT_VALUE_FLOAT,
    MIDLIT_VALUE_STR,
    MIDLIT_VALUE_ARRAY,
    MIDLIT_VALUE_STRUCT,
    MIDLIT_VALUE_UNION,
    MIDLIT_VALUE_PTR,
};

// used by values in an array or string
struct midlit_ValueArrInfo {
    uint_least64_t len;
    struct midlit_TaggedValue *elems; // non-owning
    enum midlit_ValueKind kind;       // can be either MIDLIT_VALUE_ARRAY or
                                      // MIDLIT_VALUE_STR
};

struct midlit_Ptr {
    union {
        struct midlit_TaggedValue *raw_val; // used if idx_used is false.
                                            // non-owning ptr to the value we're
                                            // pointing to
        struct midlit_ValueArrInfo arr_info; // used if idx_used is true.
                                             // info abt the array or string
                                             // we're pointing to
    };
    uint_least64_t val_idx; // idx of the value we're pointing to if idx_used
                            // is true.
    bool idx_used;
    bool past_end; // are we pointing past the end of val or the array val is
                   // in?
};

void midlit_Ptr_deinit(struct midlit_Ptr *self);
struct midlit_Ptr midlit_copy_ptr(const struct midlit_Ptr *src);
struct midlit_Ptr midlit_null_ptr();
void midlit_fprint_ptr(FILE *out, const struct midlit_Ptr *self);
void midlit_print_ptr(const struct midlit_Ptr *self);
bool midlit_ptr_is_null(const struct midlit_Ptr *self);
// returns nullptr on failure
struct midlit_TaggedValue *midlit_deref_ptr(const struct midlit_Ptr *self);
// value kind of the value pointed to
enum midlit_ValueKind midlit_deref_ptr_kind(const struct midlit_Ptr *self);
struct midlit_TaggedValue midlit_ref_val(struct midlit_TaggedValue *self);
// returns true on success, false on failure
bool midlit_inc_ptr(struct midlit_Ptr *self, int_least64_t inc);
// returns true on success, false on failure
bool midlit_dec_ptr(struct midlit_Ptr *self, int_least64_t dec);

union midlit_Value {
    struct mid_APInt i;
    struct mid_APFloat flt;

    struct midlit_String str;

    struct midlit_Array arr;

    struct midlit_Ptr ptr;

    struct midsema_StructLit struct_;
    struct midsema_UnionLit union_;
};
midgen_dynarray_struct_named(midlit_ValueVec, union midlit_Value);

void midlit_Value_deinit(union midlit_Value *self, enum midlit_ValueKind kind);

struct midlit_TaggedValue {
    struct midlit_ValueArrInfo arr_info; // only used if in_arr is true
    union midlit_Value v;
    enum midlit_ValueKind kind;
    bool in_arr; // true if the value is in an array or a string
};
midgen_dynarray_struct_named(midlit_TaggedValueVec, struct midlit_TaggedValue);

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
void midlit_convert_value(struct midlit_TaggedValue *val,
                          const struct midpar_Type *target);
// any values to be deinit-ed will be pushed to the deinit queue instead of
// being deinited right away, in case u wanna extend the values' life times
void midlit_convert_value_deinit_queue(
    struct midlit_TaggedValue *val, const struct midpar_Type *target,
    struct midlit_TaggedValueVec *deinit_queue);
bool midlit_can_convert_value(const struct midlit_TaggedValue *val,
                              const struct midpar_Type *target);

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
