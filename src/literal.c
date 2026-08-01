#include "literal.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "parser/expr_type.h"
#include "types.h"
#include "utf8.h"
#include <ctype.h>
#include <stdio.h>
#include <wchar.h>

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

static mid_isize c16_str_len(const u16 *str)
{
    mid_isize i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static mid_isize c32_str_len(const u32 *str)
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

void midlit_fprint(FILE *out, union midlit_Value val, enum midpar_ExprType type)
{
    switch (type) {
    case MIDPAR_EXPRTYPE_CHAR_LIT:
        fprintf(out, "'%c'", (char)val.sint);
        break;

    case MIDPAR_EXPRTYPE_WCHAR_LIT:
        fprintf(out, "'%C'", (wchar_t)val.sint);
        break;

    case MIDPAR_EXPRTYPE_CHAR16_LIT:
    case MIDPAR_EXPRTYPE_CHAR32_LIT:
        fputc('\'', out);
        midutf8_fprint_char(out, val.uint);
        fputc('\'', out);
        break;

    case MIDPAR_EXPRTYPE_STRING_LIT:
        fprintf(out, "\"%s\"", val.str.c);
        break;

    case MIDPAR_EXPRTYPE_WSTRING_LIT:
        fputc('"', out);
        static_assert(midtype_wchar_size == 2 || midtype_wchar_size == 4);
        if (midtype_wchar_size == 2)
            midutf8_fprint_str16(out, (void *)val.str.wc);
        else
            midutf8_fprint_str32(out, (void *)val.str.wc);
        fputc('"', out);
        break;

    case MIDPAR_EXPRTYPE_STRING16_LIT:
        fputc('"', out);
        midutf8_fprint_str16(out, val.str.c16);
        fputc('"', out);
        break;

    case MIDPAR_EXPRTYPE_STRING32_LIT:
        fputc('"', out);
        midutf8_fprint_str32(out, val.str.c32);
        fputc('"', out);
        break;

    case MIDPAR_EXPRTYPE_INT_LIT:
    case MIDPAR_EXPRTYPE_LONG_LIT:
    case MIDPAR_EXPRTYPE_LONGLONG_LIT:
        fprintf(out, "%" PRIi64, val.sint);
        break;

    case MIDPAR_EXPRTYPE_UINT_LIT:
    case MIDPAR_EXPRTYPE_ULONG_LIT:
    case MIDPAR_EXPRTYPE_ULONGLONG_LIT:
        fprintf(out, "%" PRIu64, val.uint);
        break;

    case MIDPAR_EXPRTYPE_FLOAT_LIT:
    case MIDPAR_EXPRTYPE_DOUBLE_LIT:
    case MIDPAR_EXPRTYPE_LONGDOUBLE_LIT:
        fprintf(out, "%Lf", val.flt);
        break;

    case MIDPAR_EXPRTYPE_BOOL_LIT:
        fprintf(out, "%s", val.sint ? "true" : "false");
        break;

    case MIDPAR_EXPRTYPE_NULLPTR_LIT:
        fprintf(out, "nullptr");
        break;

    default:
        MID_CRASH("expr is not a literal");
    }
}

void midlit_fprint_toktype(FILE *out, union midlit_Value val,
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

void midlit_print(union midlit_Value val, enum midpar_ExprType type)
{
    midlit_fprint(stdout, val, type);
}

void midlit_print_toktype(union midlit_Value val, enum midlex_TokenType type)
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

static u64 read_intlit_hex(const char *str, mid_isize start, mid_isize *out_end)
{
    u64 ret = 0;

    mid_isize i;
    for (i = start; is_hex_digit(str[i]); ++i) {
        ret *= 16;
        ret += hex_digit_to_num(str[i]);
    }

    if (out_end)
        *out_end = i;
    return ret;
}

static bool is_bin_digit(char c)
{
    return c == '0' || c == '1';
}

static u64 read_intlit_bin(const char *str, mid_isize start, mid_isize *out_end)
{
    u64 ret = 0;

    mid_isize i;
    for (i = start; is_bin_digit(str[i]); ++i) {
        ret *= 2;
        ret += str[i] - '0';
    }

    if (out_end)
        *out_end = i;
    return ret;
}

static bool is_octal_digit(char c)
{
    return c >= '0' && c <= '7';
}

static u64 read_intlit_octal(const char *str, mid_isize start,
                             mid_isize *out_end)
{
    u64 ret = 0;

    mid_isize i;
    for (i = start; is_octal_digit(str[i]); ++i) {
        ret *= 8;
        ret += str[i] - '0';
    }

    if (out_end)
        *out_end = i;
    return ret;
}

static u64 read_intlit_decimal(const char *str, mid_isize start,
                               mid_isize *out_end)
{
    u64 ret = 0;

    mid_isize i;
    for (i = start; isdigit(str[i]); ++i) {
        ret *= 10;
        ret += str[i] - '0';
    }

    if (out_end)
        *out_end = i;
    return ret;
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
