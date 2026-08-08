#include "literal.h"
#include "apfloat.h"
#include "apint.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/expr_type.h"
#include "sema/class_lit.h"
#include "types.h"
#include "utf8.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <wchar.h>

void midlit_Array_deinit(struct midlit_Array *self)
{
    for (uint_least64_t i = 0; i < self->len; ++i) {
        midlit_TaggedValue_deinit(&self->elems[i]);
    }
    free(self->elems);
}

void midlit_Value_deinit(union midlit_Value *self, enum midlit_ValueKind kind)
{
    switch (kind) {
    case MIDLIT_VALUE_SIGNED_INT:
    case MIDLIT_VALUE_UNSIGNED_INT:
        mid_APInt_deinit(&self->i);
        break;

    case MIDLIT_VALUE_FLOAT:
        mid_APFloat_deinit(&self->flt);
        break;

    case MIDLIT_VALUE_STR:
        MID_CRASH("string literals are deinit-ed by the str lits table");
        break;

    case MIDLIT_VALUE_ARRAY:
        midlit_Array_deinit(&self->arr);
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

struct midlit_Array midlit_copy_array(const struct midlit_Array *src)
{
    struct midlit_Array dest = *src;
    dest.elems = mid_malloc(src->len * sizeof(*dest.elems));

    for (uint_least64_t i = 0; i < src->len; ++i)
        dest.elems[i] = midlit_copy_value(&src->elems[i]);

    return dest;
}

struct midlit_TaggedValue
midlit_copy_value(const struct midlit_TaggedValue *src)
{
    struct midlit_TaggedValue ret = {.kind = src->kind};

    switch (src->kind) {
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

    case MIDLIT_VALUE_STRUCT:
        MID_CRASH("copying struct values not implemented yet");
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
}

static mid_isize c_str_len(const TypesCharType *str)
{
    mid_isize i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static mid_isize wc_str_len(const TypesWCharType *str)
{
    mid_isize i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static mid_isize c16_str_len(const uint16_t *str)
{
    mid_isize i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static mid_isize c32_str_len(const uint32_t *str)
{
    mid_isize i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

mid_isize midlit_strlit_len(const struct midlit_String *strlit)
{
    switch (strlit->type) {
    case MIDLIT_STRINGTYPE_CHAR:
        return c_str_len(strlit->c);

    case MIDLIT_STRINGTYPE_WCHAR:
        return wc_str_len(strlit->wc);

    case MIDLIT_STRINGTYPE_CHAR16:
        return c16_str_len(strlit->c16);

    case MIDLIT_STRINGTYPE_CHAR32:
        return c32_str_len(strlit->c32);
    }
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

void midlit_tagged_fprint(FILE *out, const struct midlit_TaggedValue *val)
{
    switch (val->kind) {
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
