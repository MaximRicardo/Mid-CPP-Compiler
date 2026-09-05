#include "literal.h"
#include "apfloat.h"
#include "apint.h"
#include "cmd.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/class.h"
#include "parser/expr_type.h"
#include "sema/class_lit.h"
#include "sema/ident.h"
#include "sema/type.h"
#include "types.h"
#include "utf8.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

void midlit_Array_deinit(struct midlit_Array *self)
{
    for (uint_least64_t i = 0; i < self->len; ++i) {
        midlit_TaggedValue_deinit(&self->elems[i]);
    }
    free(self->elems);
}

void midlit_Ptr_deinit(struct midlit_Ptr *self)
{
    (void)self;
    /*
    if (!self->idx_used) {
        if (self->raw_val) {
            midlit_TaggedValue_deinit(self->raw_val);
            free(self->raw_val);
        }
    }
    */
}

void midlit_Value_deinit(union midlit_Value *self, enum midlit_ValueKind kind)
{
    switch (kind) {
    case MIDLIT_VALUE_NONE:
        break;

    case MIDLIT_VALUE_SIGNED_INT:
    case MIDLIT_VALUE_UNSIGNED_INT:
        mid_APInt_deinit(&self->i);
        break;

    case MIDLIT_VALUE_FLOAT:
        mid_APFloat_deinit(&self->flt);
        break;

    case MIDLIT_VALUE_STR:
        break;

    case MIDLIT_VALUE_ARRAY:
        midlit_Array_deinit(&self->arr);
        break;

    case MIDLIT_VALUE_PTR:
        midlit_Ptr_deinit(&self->ptr);
        break;

    case MIDLIT_VALUE_STRUCT:
        midsema_StructLit_deinit(&self->struct_);
        break;

    case MIDLIT_VALUE_UNION:
        midsema_UnionLit_deinit(&self->union_);
        break;
    }
}

void midlit_TaggedValue_deinit(struct midlit_TaggedValue *self)
{
    midlit_Value_deinit(&self->v, self->kind);
}

static bool str_type_signed(enum midlit_StringType type)
{
    if (type == MIDLIT_STRINGTYPE_CHAR)
        return midtype_char_signed;
    else if (type == MIDLIT_STRINGTYPE_WCHAR)
        return midtype_wchar_signed;
    else
        return false;
}

static struct midlit_ValueArrInfo str_arr_info(struct midlit_String *self)
{
    // len is self->len + 1 cuz we gotta account for the null terminator
    return (struct midlit_ValueArrInfo){
        .len = self->len + 1, .elems = self->nums, .kind = MIDLIT_VALUE_STR};
}

void midlit_setup_string_nums(struct midlit_String *self)
{
    self->nums = mid_malloc((self->len + 1) * sizeof(*self->nums));

    bool is_signed = str_type_signed(self->type);
    enum midlit_ValueKind kind =
        is_signed ? MIDLIT_VALUE_SIGNED_INT : MIDLIT_VALUE_UNSIGNED_INT;
    int32_t bits = midlit_strtype_char_size(self->type) * 8;

    for (uint_least64_t i = 0; i < self->len; ++i) {
        self->nums[i].kind = kind;
        self->nums[i].in_arr = true;
        self->nums[i].arr_info = str_arr_info(self);

        switch (self->type) {
        case MIDLIT_STRINGTYPE_CHAR:
            self->nums[i].v.i = midint_init(bits, self->c[i], is_signed);
            break;

        case MIDLIT_STRINGTYPE_WCHAR:
            self->nums[i].v.i = midint_init(bits, self->wc[i], is_signed);
            break;

        case MIDLIT_STRINGTYPE_CHAR16:
            self->nums[i].v.i = midint_init(bits, self->c16[i], is_signed);
            break;

        case MIDLIT_STRINGTYPE_CHAR32:
            self->nums[i].v.i = midint_init(bits, self->c32[i], is_signed);
            break;
        }
    }

    self->nums[self->len].kind = kind;
    self->nums[self->len].v.i = midint_zero(bits);
}

struct midlit_Array midlit_copy_array(const struct midlit_Array *src)
{
    struct midlit_Array dest = *src;
    dest.elems = mid_malloc(src->len * sizeof(*dest.elems));

    for (uint_least64_t i = 0; i < src->len; ++i)
        dest.elems[i] = midlit_copy_value(&src->elems[i]);

    return dest;
}

struct midlit_Ptr midlit_copy_ptr(const struct midlit_Ptr *src)
{
    struct midlit_Ptr dest = *src;

    /*
    if (!src->idx_used && src->raw_val) {
        dest.raw_val = mid_malloc(sizeof(*dest.raw_val));
        *dest.raw_val = midlit_copy_value(src->raw_val);
    }
    */

    return dest;
}

struct midlit_Ptr midlit_null_ptr()
{
    return (struct midlit_Ptr){
        .raw_val = nullptr, .idx_used = false, .past_end = false};
}

struct midlit_TaggedValue
midlit_copy_value(const struct midlit_TaggedValue *src)
{
    struct midlit_TaggedValue ret = {.kind = src->kind};

    switch (src->kind) {
    case MIDLIT_VALUE_NONE:
        break;

    case MIDLIT_VALUE_SIGNED_INT:
    case MIDLIT_VALUE_UNSIGNED_INT:
        ret.v.i = midint_copy(&src->v.i);
        break;

    case MIDLIT_VALUE_FLOAT:
        ret.v.flt = midflt_copy(&src->v.flt);
        break;

    case MIDLIT_VALUE_STR:
        ret.v.str = src->v.str;
        break;

    case MIDLIT_VALUE_ARRAY:
        ret.v.arr = midlit_copy_array(&src->v.arr);
        break;

    case MIDLIT_VALUE_PTR:
        ret.v.ptr = midlit_copy_ptr(&src->v.ptr);
        break;

    case MIDLIT_VALUE_STRUCT:
        ret.v.struct_ = midsema_copy_structlit(&src->v.struct_);
        break;

    case MIDLIT_VALUE_UNION:
        MID_CRASH("copying union values not implemented yet");
        break;
    }

    return ret;
}

int midlit_strtype_char_size(enum midlit_StringType type)
{
    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        return midtype_char_size;

    case MIDLIT_STRINGTYPE_WCHAR:
        return midtype_wchar_size;

    case MIDLIT_STRINGTYPE_CHAR16:
        return 2;

    case MIDLIT_STRINGTYPE_CHAR32:
        return 4;
    }
}

void midlit_String_deinit(struct midlit_String *self)
{
    switch (self->type) {
    case MIDLIT_STRINGTYPE_CHAR:
        free(self->c);
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        free(self->wc);
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        free(self->c16);
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        free(self->c32);
        break;
    }

    for (uint_least64_t i = 0; i <= self->len; ++i) {
        midlit_TaggedValue_deinit(&self->nums[i]);
    }
    free(self->nums);
}

struct midlit_TaggedValue *midlit_deref_ptr(const struct midlit_Ptr *self)
{
    if (self->past_end)
        return nullptr;

    if (!self->idx_used)
        return self->raw_val;
    else
        return &self->arr_info.elems[self->val_idx];
}

enum midlit_ValueKind midlit_deref_ptr_kind(const struct midlit_Ptr *self)
{
    assert(!midlit_ptr_is_null(self));

    if (self->idx_used) {
        return self->arr_info.elems[0].kind;
    } else {
        return self->raw_val->kind;
    }
}

bool midlit_ptr_is_null(const struct midlit_Ptr *self)
{
    if (self->idx_used) {
        return !self->arr_info.elems;
    } else {
        return !self->raw_val;
    }
}

struct midlit_TaggedValue midlit_ref_val(struct midlit_TaggedValue *self)
{
    struct midlit_TaggedValue ret = {.kind = MIDLIT_VALUE_PTR};
    struct midlit_Ptr *ptr = &ret.v.ptr;
    ptr->idx_used = self->in_arr;
    ptr->past_end = false;

    if (self->in_arr) {
        ptr->arr_info = self->arr_info;
        ptr->val_idx = self - self->arr_info.elems;
        assert(ptr->val_idx < ptr->arr_info.len);
    } else {
        ptr->raw_val = self;
    }

    return ret;
}

// TODO: delete this cuz strings store their length now
mid_isize midlit_strlit_len(const struct midlit_String *strlit)
{
    return strlit->len;
}

void midlit_fprint_strlit(FILE *out, const struct midlit_String *self)
{
    switch (self->type) {
    case MIDLIT_STRINGTYPE_CHAR:
        fprintf(out, "\"%s\"", self->c);
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        fputc('"', out);
        static_assert(midtype_wchar_size == 2 || midtype_wchar_size == 4);
        if (midtype_wchar_size == 2)
            midutf8_fprint_str16(out, (void *)self->wc);
        else
            midutf8_fprint_str32(out, (void *)self->wc);
        fputc('"', out);
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        fputc('"', out);
        midutf8_fprint_str16(out, self->c16);
        fputc('"', out);
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        fputc('"', out);
        midutf8_fprint_str32(out, self->c32);
        fputc('"', out);
        break;
    }
}

void midlit_print_strlit(const struct midlit_String *self)
{
    midlit_fprint_strlit(stdout, self);
}

void midlit_fprint_array(FILE *out, const struct midlit_Array *self)
{
    fprintf(out, "{");

    for (uint_least64_t i = 0; i < self->len; ++i) {
        if (i > 0)
            fprintf(out, ", ");
        midlit_tagged_fprint(out, &self->elems[i]);
    }

    fprintf(out, "}");
}

void midlit_print_array(const struct midlit_Array *self)
{
    midlit_fprint_array(stdout, self);
}

void midlit_fprint_ptr(FILE *out, const struct midlit_Ptr *self)
{
    if (midlit_ptr_is_null(self)) {
        fprintf(out, "(null ptr)");
        return;
    } else if (self->past_end) {
        fprintf(out, "(invalid ptr)");
        return;
    }

    fprintf(out, "ptr to ");
    midlit_tagged_fprint(out, midlit_deref_ptr(self));
}

void midlit_print_ptr(const struct midlit_Ptr *self)
{
    midlit_fprint_ptr(stdout, self);
}

void midlit_tagged_fprint(FILE *out, const struct midlit_TaggedValue *val)
{
    switch (val->kind) {
    case MIDLIT_VALUE_NONE:
        fprintf(out, "(none)");
        break;

    case MIDLIT_VALUE_SIGNED_INT:
        midint_print(&val->v.i, out, true);
        break;

    case MIDLIT_VALUE_UNSIGNED_INT:
        midint_print(&val->v.i, out, false);
        break;

    case MIDLIT_VALUE_FLOAT:
        midflt_print(&val->v.flt, out);
        break;

    case MIDLIT_VALUE_STR:
        midlit_fprint_strlit(out, &val->v.str);
        break;

    case MIDLIT_VALUE_ARRAY:
        midlit_fprint_array(out, &val->v.arr);
        break;

    case MIDLIT_VALUE_PTR:
        midlit_fprint_ptr(out, &val->v.ptr);
        break;

    case MIDLIT_VALUE_STRUCT:
        midsema_fprint_structlit(out, &val->v.struct_);
        break;

    case MIDLIT_VALUE_UNION:
        MID_CRASH("printing unions not implemented yet");
        break;
    }
}

void midlit_tagged_print(const struct midlit_TaggedValue *val)
{
    midlit_tagged_fprint(stdout, val);
}

void midlit_fprint(FILE *out, const union midlit_Value *val,
                   enum midpar_ExprType type)
{
    switch (type) {
    case MIDPAR_EXPRTYPE_CHAR_LIT:
        fprintf(out, "'%c'", (char)midint_to_uint(&val->i));
        break;

    case MIDPAR_EXPRTYPE_WCHAR_LIT:
        fprintf(out, "'%C'", (wchar_t)midint_to_uint(&val->i));
        break;

    case MIDPAR_EXPRTYPE_CHAR16_LIT:
    case MIDPAR_EXPRTYPE_CHAR32_LIT:
        fputc('\'', out);
        midutf8_fprint_char(out, midint_to_uint(&val->i));
        fputc('\'', out);
        break;

    case MIDPAR_EXPRTYPE_STRING_LIT:
    case MIDPAR_EXPRTYPE_WSTRING_LIT:
    case MIDPAR_EXPRTYPE_STRING16_LIT:
    case MIDPAR_EXPRTYPE_STRING32_LIT:
        midlit_fprint_strlit(out, &val->str);

    case MIDPAR_EXPRTYPE_INT_LIT:
    case MIDPAR_EXPRTYPE_LONG_LIT:
    case MIDPAR_EXPRTYPE_LONGLONG_LIT:
        fprintf(out, "%" PRIi64, midint_to_sint(&val->i));
        break;

    case MIDPAR_EXPRTYPE_UINT_LIT:
    case MIDPAR_EXPRTYPE_ULONG_LIT:
    case MIDPAR_EXPRTYPE_ULONGLONG_LIT:
        fprintf(out, "%" PRIu64, midint_to_uint(&val->i));
        break;

    case MIDPAR_EXPRTYPE_FLOAT_LIT:
    case MIDPAR_EXPRTYPE_DOUBLE_LIT:
    case MIDPAR_EXPRTYPE_LONGDOUBLE_LIT:
        fprintf(out, "%lf", midflt_to_dbl(&val->flt));
        break;

    case MIDPAR_EXPRTYPE_BOOL_LIT:
        fprintf(out, "%s", midint_is_zero(&val->i) ? "false" : "true");
        break;

    case MIDPAR_EXPRTYPE_NULLPTR_LIT:
        fprintf(out, "nullptr");
        break;

    default:
        MID_CRASH("expr is not a literal");
    }
}

void midlit_fprint_toktype(FILE *out, const union midlit_Value *val,
                           enum midlex_TokenType type)
{
    switch (type) {
    case MIDLEX_TOKENTYPE_CHAR_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_CHAR_LIT);
        break;

    case MIDLEX_TOKENTYPE_WCHAR_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_WCHAR_LIT);
        break;

    case MIDLEX_TOKENTYPE_CHAR16_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_CHAR16_LIT);
        break;

    case MIDLEX_TOKENTYPE_CHAR32_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_CHAR32_LIT);
        break;

    case MIDLEX_TOKENTYPE_STRING_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_STRING_LIT);
        break;

    case MIDLEX_TOKENTYPE_WSTRING_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_WSTRING_LIT);
        break;

    case MIDLEX_TOKENTYPE_STRING16_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_STRING16_LIT);
        break;

    case MIDLEX_TOKENTYPE_STRING32_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_STRING32_LIT);
        break;

    case MIDLEX_TOKENTYPE_INT_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_INT_LIT);
        break;

    case MIDLEX_TOKENTYPE_UINT_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_UINT_LIT);
        break;

    case MIDLEX_TOKENTYPE_LONG_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_LONG_LIT);
        break;

    case MIDLEX_TOKENTYPE_ULONG_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_ULONG_LIT);
        break;

    case MIDLEX_TOKENTYPE_LONGLONG_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_LONGLONG_LIT);
        break;

    case MIDLEX_TOKENTYPE_ULONGLONG_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_ULONGLONG_LIT);
        break;

    case MIDLEX_TOKENTYPE_FLOAT_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_FLOAT_LIT);
        break;

    case MIDLEX_TOKENTYPE_DOUBLE_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_DOUBLE_LIT);
        break;

    case MIDLEX_TOKENTYPE_LONGDOUBLE_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_LONGDOUBLE_LIT);
        break;

    case MIDLEX_TOKENTYPE_BOOL_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_BOOL_LIT);
        break;

    case MIDLEX_TOKENTYPE_NULLPTR_LIT:
        midlit_fprint(out, val, MIDPAR_EXPRTYPE_NULLPTR_LIT);
        break;

    default:
        MID_CRASH("token is not literal");
    }
}

void midlit_print(const union midlit_Value *val, enum midpar_ExprType type)
{
    midlit_fprint(stdout, val, type);
}

void midlit_print_toktype(const union midlit_Value *val,
                          enum midlex_TokenType type)
{
    midlit_fprint_toktype(stdout, val, type);
}

static bool is_hex_digit(char c)
{
    return isdigit(c) || c == 'a' || c == 'b' || c == 'c' || c == 'd' ||
           c == 'e' || c == 'f' || c == 'A' || c == 'B' || c == 'C' ||
           c == 'D' || c == 'E' || c == 'F';
}

static int hex_digit_to_num(char c)
{
    if (isdigit(c))
        return c - '0';

    // ASCII isn't guaranteed
    switch (c) {
    case 'a':
    case 'A':
        return 0xa;

    case 'b':
    case 'B':
        return 0xb;

    case 'c':
    case 'C':
        return 0xc;

    case 'd':
    case 'D':
        return 0xd;

    case 'e':
    case 'E':
        return 0xe;

    case 'f':
    case 'F':
        return 0xf;

    default:
        MID_CRASH("not a hex digit");
    }
}

static bool is_bin_digit(char c)
{
    return c == '0' || c == '1';
}

static bool is_octal_digit(char c)
{
    return c >= '0' && c <= '7';
}

static bool is_dec_digit(char c)
{
    return isdigit(c);
}

static bool is_valid_digit(char c, int base)
{
    if (base == 2)
        return is_bin_digit(c);
    else if (base == 8)
        return is_octal_digit(c);
    else if (base == 10)
        return is_dec_digit(c);
    else if (base == 16)
        return is_hex_digit(c);
    else
        MID_CRASH("unsupported base");
}

static void read_intlit_overflow(struct mid_APInt *accum,
                                 struct mid_APInt *prev,
                                 struct mid_APInt *div_res, int base)
{
    midint_assign(accum, prev);

    // we need to increment the width by at least enough to hold the result of
    // the next multiplication, tho preferably more
    int32_t bits_inc = MID_MAX(midtype_longlong_size * 8, ceil(log2(base)));
    int32_t new_bits = accum->n_bits + bits_inc;

    midint_ext(accum, new_bits, false);
    midint_ext(prev, new_bits, false);
    midint_ext(div_res, new_bits, false);
}

static struct mid_APInt read_intlit_common(const char *str, mid_isize start,
                                           mid_isize *out_end, int base)
{
    auto accum = midint_zero(midtype_longlong_size * 8);
    auto prev = midint_copy(&accum);

    // cached to prevent unnecessary allocations
    auto div_res = midint_alloc(accum.n_bits);

    mid_isize i;
    for (i = start; is_valid_digit(str[i], base); ++i) {
        midint_mul_uimm(&accum, base);

        // detecting mul overflow
        // let x = a * b,
        // if a != 0 and x / a != b then the multiplication overflowed
        if (!midint_is_zero(&prev)) {
            midint_assign(&div_res, &accum);
            midint_udiv(&div_res, &prev);
            if (!midint_is_eq_uimm(&div_res, base)) {
                read_intlit_overflow(&accum, &prev, &div_res, base);
                --i;
                continue;
            }
        }

        midint_assign(&prev, &accum);

        midint_add_uimm(&accum,
                        base == 16 ? hex_digit_to_num(str[i]) : str[i] - '0');

        if (midint_is_ult(&accum, &prev)) {
            read_intlit_overflow(&accum, &prev, &div_res, base);
            --i;
            continue;
        }

        midint_assign(&prev, &accum);
    }

    mid_APInt_deinit(&div_res);
    mid_APInt_deinit(&prev);

    if (out_end)
        *out_end = i;
    return accum;
}

static struct mid_APInt read_intlit_hex(const char *str, mid_isize start,
                                        mid_isize *out_end)
{
    return read_intlit_common(str, start, out_end, 16);
}

static struct mid_APInt read_intlit_bin(const char *str, mid_isize start,
                                        mid_isize *out_end)
{
    return read_intlit_common(str, start, out_end, 2);
}

static struct mid_APInt read_intlit_octal(const char *str, mid_isize start,
                                          mid_isize *out_end)
{
    return read_intlit_common(str, start, out_end, 8);
}

static struct mid_APInt read_intlit_decimal(const char *str, mid_isize start,
                                            mid_isize *out_end)
{
    return read_intlit_common(str, start, out_end, 10);
}

struct midlit_ReadIntLitInfo
midlit_read_intlit(const char *str, mid_isize start, mid_isize *out_end)
{
    assert(isdigit(str[start]));

    struct midlit_ReadIntLitInfo ret = {};

    if (str[start] == '0') {
        if (str[start + 1] == 'x') {
            ret.base = 16;
            ret.value = read_intlit_hex(str, start + 2, out_end);
        } else if (str[start + 1] == 'b') {
            ret.base = 2;
            ret.value = read_intlit_bin(str, start + 2, out_end);
        } else {
            ret.base = 8;
            ret.value = read_intlit_octal(str, start + 1, out_end);
        }
    } else {
        ret.base = 10;
        ret.value = read_intlit_decimal(str, start, out_end);
    }

    return ret;
}

// returns true on success, false on failure
static bool inc_ptr_with_idx(struct midlit_Ptr *self, int_least64_t inc)
{
    uint_least64_t new_idx = self->val_idx + inc;
    bool overflow = inc > 0 ? new_idx < self->val_idx : new_idx > self->val_idx;
    if (overflow)
        return false;

    if (new_idx > self->arr_info.len)
        return false;
    else if (new_idx == self->arr_info.len)
        self->past_end = true;
    else
        self->val_idx = new_idx;

    return true;
}

bool midlit_inc_ptr(struct midlit_Ptr *self, int_least64_t inc)
{
    if (self->idx_used)
        return inc_ptr_with_idx(self, inc);

    if (inc == 0) {
        return true;
    } else if (inc == 1) {
        if (self->past_end)
            return false;
        self->past_end = true;
        return true;
    } else {
        return false;
    }
}

bool midlit_dec_ptr(struct midlit_Ptr *self, int_least64_t dec)
{
    return midlit_inc_ptr(self, -dec);
}

static void convert_str_value(struct midlit_TaggedValue *str,
                              enum midlit_ValueKind target_kind,
                              struct midlit_TaggedValueVec *deinit_queue)
{
    if (target_kind == MIDLIT_VALUE_STR)
        return;
    else if (target_kind != MIDLIT_VALUE_PTR)
        MID_CRASH("can't convert a string to a non-ptr type");

    midgen_dynpush(deinit_queue, *str);

    *str = midlit_ref_val(&str->v.str.nums[0]);
}

static bool can_convert_str_value(enum midlit_ValueKind target_kind)
{
    return target_kind == MIDLIT_VALUE_STR || target_kind == MIDLIT_VALUE_PTR;
}

static void convert_arr_value(struct midlit_TaggedValue *arr,
                              enum midlit_ValueKind target_kind,
                              struct midlit_TaggedValueVec *deinit_queue)
{
    if (target_kind == MIDLIT_VALUE_ARRAY)
        return;
    else if (target_kind != MIDLIT_VALUE_PTR)
        MID_CRASH("can't convert an array to a non-ptr type");

    midgen_dynpush(deinit_queue, *arr);

    *arr = midlit_ref_val(&arr->v.arr.elems[0]);
}

static bool can_convert_arr_value(enum midlit_ValueKind target_kind)
{
    return target_kind == MIDLIT_VALUE_ARRAY || target_kind == MIDLIT_VALUE_PTR;
}

static bool can_convert_class_value(const struct midlit_TaggedValue *val,
                                    const struct midpar_Type *target)
{
    const struct midpar_Class *class = val->kind == MIDLIT_VALUE_STRUCT
                                           ? val->v.struct_.class_
                                           : val->v.union_.class_;

    return midsema_deref_identptr(&class->ident) ==
           midsema_deref_identptr(&target->named);
}

void midlit_convert_value(struct midlit_TaggedValue *val,
                          const struct midpar_Type *target)
{
    struct midlit_TaggedValueVec deinit_queue = {};

    midlit_convert_value_deinit_queue(val, target, &deinit_queue);

    midgen_dyndeinit(&deinit_queue, midlit_TaggedValue_deinit);
}

void midlit_convert_value_deinit_queue(
    struct midlit_TaggedValue *val, const struct midpar_Type *target,
    struct midlit_TaggedValueVec *deinit_queue)
{
    enum midlit_ValueKind target_kind = midsema_type_lit_value_kind(target);
    enum midflt_Kind target_flt_kind =
        midsema_is_floating_typespec(target->spec)
            ? midsema_get_flt_kind(target->spec)
            : -1;

    switch (val->kind) {
    case MIDLIT_VALUE_NONE:
        if (target_kind != MIDLIT_VALUE_NONE) {
            MID_CRASH("can't convert none");
        }
        break;

    case MIDLIT_VALUE_SIGNED_INT:
    case MIDLIT_VALUE_UNSIGNED_INT: {
        int_least64_t target_width = midsema_typespec_size(target->spec) * 8;
        switch (target_kind) {
        case MIDLIT_VALUE_NONE:
            MID_CRASH("can't convert integer to none");

        case MIDLIT_VALUE_SIGNED_INT:
            midint_ext(&val->v.i, target_width, true);
            break;

        case MIDLIT_VALUE_UNSIGNED_INT:
            midint_ext(&val->v.i, target_width, false);
            break;

        case MIDLIT_VALUE_FLOAT: {
            auto tmp = val->kind == MIDLIT_VALUE_SIGNED_INT
                           ? midflt_init_sint(&val->v.i, target_flt_kind,
                                              midcmd_get_fpu()->rmode)
                           : midflt_init_uint(&val->v.i, target_flt_kind,
                                              midcmd_get_fpu()->rmode);
            mid_APInt_deinit(&val->v.i);
            val->v.flt = tmp;
        } break;

        case MIDLIT_VALUE_STR:
            MID_CRASH("can't convert integer to string");

        case MIDLIT_VALUE_ARRAY:
            MID_CRASH("can't convert integer to array");

        case MIDLIT_VALUE_STRUCT:
            MID_CRASH("can't convert integer to struct");

        case MIDLIT_VALUE_UNION:
            MID_CRASH("can't convert integer to union");

        case MIDLIT_VALUE_PTR:
            MID_CRASH("can't convert integer to ptr");
        }
        break;
    }

    case MIDLIT_VALUE_FLOAT: {
        int_least64_t target_width = midsema_typespec_size(target->spec) * 8;
        switch (target_kind) {
        case MIDLIT_VALUE_NONE:
            MID_CRASH("can't convert float to none");

        case MIDLIT_VALUE_SIGNED_INT:
        case MIDLIT_VALUE_UNSIGNED_INT: {
            auto tmp = midflt_to_sint(&val->v.flt);
            mid_APFloat_deinit(&val->v.flt);
            val->v.i = tmp;
            midint_ext(&val->v.i, target_width,
                       target_kind == MIDLIT_VALUE_SIGNED_INT);
        } break;

        case MIDLIT_VALUE_FLOAT:
            midflt_change_kind(&val->v.flt, target_flt_kind);
            break;

        case MIDLIT_VALUE_STR:
            MID_CRASH("can't convert float to string");

        case MIDLIT_VALUE_ARRAY:
            MID_CRASH("can't convert float to array");

        case MIDLIT_VALUE_STRUCT:
            MID_CRASH("can't convert float to struct");

        case MIDLIT_VALUE_UNION:
            MID_CRASH("can't convert float to union");

        case MIDLIT_VALUE_PTR:
            MID_CRASH("can't convert float to ptr");
        }
        break;
    }

    case MIDLIT_VALUE_STR:
        convert_str_value(val, target_kind, deinit_queue);
        break;

    case MIDLIT_VALUE_ARRAY:
        convert_arr_value(val, target_kind, deinit_queue);
        break;

    case MIDLIT_VALUE_STRUCT:
        if (target_kind != MIDLIT_VALUE_STRUCT)
            MID_CRASH("can't convert structs");
        break;

    case MIDLIT_VALUE_UNION:
        if (target_kind != MIDLIT_VALUE_UNION)
            MID_CRASH("can't convert unions");
        break;

    case MIDLIT_VALUE_PTR:
        if (target_kind != MIDLIT_VALUE_PTR)
            MID_CRASH("can't convert ptrs");
        break;
    }

    val->kind = target_kind;
}

bool midlit_can_convert_value(const struct midlit_TaggedValue *val,
                              const struct midpar_Type *target)
{
    enum midlit_ValueKind target_kind = midsema_type_lit_value_kind(target);

    switch (val->kind) {
    case MIDLIT_VALUE_NONE:
        return false;

    case MIDLIT_VALUE_SIGNED_INT:
    case MIDLIT_VALUE_UNSIGNED_INT:
        return target_kind == MIDLIT_VALUE_SIGNED_INT ||
               target_kind == MIDLIT_VALUE_UNSIGNED_INT ||
               target_kind == MIDLIT_VALUE_FLOAT;

    case MIDLIT_VALUE_FLOAT:
        return target_kind == MIDLIT_VALUE_SIGNED_INT ||
               target_kind == MIDLIT_VALUE_UNSIGNED_INT ||
               target_kind == MIDLIT_VALUE_FLOAT;

    case MIDLIT_VALUE_STR:
        return can_convert_str_value(target_kind);

    case MIDLIT_VALUE_ARRAY:
        return can_convert_arr_value(target_kind);

    case MIDLIT_VALUE_UNION:
    case MIDLIT_VALUE_STRUCT:
        return can_convert_class_value(val, target);

    case MIDLIT_VALUE_PTR:
        return target_kind == MIDLIT_VALUE_PTR;
    }
}
