#include "lexer/tokenize.h"
#include "apfloat.h"
#include "apint.h"
#include "cmd.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "literal.h"
#include "macros.h"
#include "mid_alloc.h"
#include "position.h"
#include "symbol.h"
#include "types.h"
#include "utf8.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// expands to a chain of case statements
#define CASE_ISALPHA                                                           \
    case 'a':                                                                  \
    case 'b':                                                                  \
    case 'c':                                                                  \
    case 'd':                                                                  \
    case 'e':                                                                  \
    case 'f':                                                                  \
    case 'g':                                                                  \
    case 'h':                                                                  \
    case 'i':                                                                  \
    case 'j':                                                                  \
    case 'k':                                                                  \
    case 'l':                                                                  \
    case 'm':                                                                  \
    case 'n':                                                                  \
    case 'o':                                                                  \
    case 'p':                                                                  \
    case 'q':                                                                  \
    case 'r':                                                                  \
    case 's':                                                                  \
    case 't':                                                                  \
    case 'u':                                                                  \
    case 'v':                                                                  \
    case 'w':                                                                  \
    case 'x':                                                                  \
    case 'y':                                                                  \
    case 'z':                                                                  \
    case 'A':                                                                  \
    case 'B':                                                                  \
    case 'C':                                                                  \
    case 'D':                                                                  \
    case 'E':                                                                  \
    case 'F':                                                                  \
    case 'G':                                                                  \
    case 'H':                                                                  \
    case 'I':                                                                  \
    case 'J':                                                                  \
    case 'K':                                                                  \
    case 'L':                                                                  \
    case 'M':                                                                  \
    case 'N':                                                                  \
    case 'O':                                                                  \
    case 'P':                                                                  \
    case 'Q':                                                                  \
    case 'R':                                                                  \
    case 'S':                                                                  \
    case 'T':                                                                  \
    case 'U':                                                                  \
    case 'V':                                                                  \
    case 'W':                                                                  \
    case 'X':                                                                  \
    case 'Y':                                                                  \
    case 'Z'

static struct midlex_Token create_basic_tok(enum midlex_TokenType type,
                                            struct mid_Position pos,
                                            const char *line)
{
    return (struct midlex_Token){.type = type, .pos = pos, .line = line};
}

static bool valid_numlit_char(char c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
           c == '.';
}

static bool valid_decimallit_char(char c)
{
    return isdigit(c) || c == '.';
}

static mid_isize find_numlit_digits_end(const char *src, mid_isize start)
{
    bool is_decimal = src[start] == '.';

    mid_isize end;
    // account for hex and binary literals
    if (src[start + 1] == 'x' || src[start + 1] == 'b')
        end = start + 2;
    else
        end = start + 1;

    while (src[end] != '\0') {
        is_decimal |= src[end] == '.';
        if (is_decimal) {
            if (!valid_decimallit_char(src[end]))
                break;
        } else {
            if (!valid_numlit_char(src[end]))
                break;
        }
        ++end;
    }

    return end;
}

enum NumLitType {
    NUMLIT_INT,
    NUMLIT_UINT,
    NUMLIT_LONG,
    NUMLIT_ULONG,
    NUMLIT_LONGLONG,
    NUMLIT_ULONGLONG,
    NUMLIT_FLOAT,
    NUMLIT_DOUBLE,
    NUMLIT_LONGDOUBLE,
};

// end is the end of the digits
// suffix_end is the end of the suffixes following, can be NULL to ignore
static enum NumLitType numlit_type(const char *src, mid_isize end,
                                   bool is_decimal, mid_isize *suffix_end)
{
    char c0 = tolower(src[end]);
    char c1 = c0 == '\0' ? '\0' : tolower(src[end + 1]);
    char c2 = c1 == '\0' ? '\0' : tolower(src[end + 2]);

    if (c0 == 'u') {
        if (c1 == 'l') {
            if (c2 == 'l') {
                if (suffix_end)
                    *suffix_end = end + 3;
                return NUMLIT_ULONGLONG;
            }
            if (suffix_end)
                *suffix_end = end + 2;
            return NUMLIT_ULONG;
        }
        if (suffix_end)
            *suffix_end = end + 1;
        return NUMLIT_UINT;
    } else if (c0 == 'l') {
        if (c1 == 'l') {
            if (suffix_end)
                *suffix_end = end + 2;
            return NUMLIT_LONGLONG;
        }
        if (suffix_end)
            *suffix_end = end + 1;
        return is_decimal ? NUMLIT_LONGDOUBLE : NUMLIT_LONG;
    } else if (c0 == 'f') {
        if (suffix_end)
            *suffix_end = end + 1;
        return NUMLIT_FLOAT;
    } else {
        if (suffix_end)
            *suffix_end = end;
        return is_decimal ? NUMLIT_DOUBLE : NUMLIT_INT;
    }
}

static bool numlit_is_decimal(const char *src, mid_isize start, mid_isize end)
{
    for (mid_isize i = start; i < end; ++i) {
        if (src[i] == '.')
            return true;
    }

    return false;
}

struct NumLit {
    union midlit_Value val;
    enum NumLitType type;
};

static struct mid_Diag intlit_too_big_err(struct mid_Position pos,
                                          const char *line)
{
    return (struct mid_Diag){
        .pos = pos,
        .line = line,
        .msg = midcmd_fmt_to_str("integer literal too big"),
        .err = MIDDIAG_ERR_BAD_LITERAL,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static enum NumLitType sel_numlit_type_int(const struct mid_APInt *val,
                                           int base, struct mid_Position pos,
                                           const char *line,
                                           struct mid_DiagVec *diags)
{
    if (base == 10) {
        if (midint_is_ulteq_imm(val, midtype_int_smax)) {
            return NUMLIT_INT;
        } else if (midint_is_ulteq_imm(val, midtype_long_smax)) {
            return NUMLIT_LONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_smax)) {
            return NUMLIT_LONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_LONGLONG;
        }
    } else {
        if (midint_is_ulteq_imm(val, midtype_int_smax)) {
            return NUMLIT_INT;
        } else if (midint_is_ulteq_imm(val, midtype_int_umax)) {
            return NUMLIT_UINT;
        } else if (midint_is_ulteq_imm(val, midtype_long_smax)) {
            return NUMLIT_LONG;
        } else if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_smax)) {
            return NUMLIT_LONGLONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type_uint(const struct mid_APInt *val,
                                            int base, struct mid_Position pos,
                                            const char *line,
                                            struct mid_DiagVec *diags)
{
    if (base == 10) {
        if (midint_is_ulteq_imm(val, midtype_int_umax)) {
            return NUMLIT_UINT;
        } else if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    } else {
        if (midint_is_ulteq_imm(val, midtype_int_umax)) {
            return NUMLIT_UINT;
        } else if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type_long(const struct mid_APInt *val,
                                            int base, struct mid_Position pos,
                                            const char *line,
                                            struct mid_DiagVec *diags)
{
    if (base == 10) {
        if (midint_is_ulteq_imm(val, midtype_long_smax)) {
            return NUMLIT_LONG;
        } else if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_smax)) {
            return NUMLIT_LONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_LONGLONG;
        }
    } else {
        if (midint_is_ulteq_imm(val, midtype_long_smax)) {
            return NUMLIT_LONG;
        } else if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_smax)) {
            return NUMLIT_LONGLONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type_ulong(const struct mid_APInt *val,
                                             int base, struct mid_Position pos,
                                             const char *line,
                                             struct mid_DiagVec *diags)
{
    if (base == 10) {
        if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    } else {
        if (midint_is_ulteq_imm(val, midtype_long_umax)) {
            return NUMLIT_ULONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type_longlong(const struct mid_APInt *val,
                                                int base,
                                                struct mid_Position pos,
                                                const char *line,
                                                struct mid_DiagVec *diags)
{
    if (base == 10) {
        if (midint_is_ulteq_imm(val, midtype_longlong_smax)) {
            return NUMLIT_LONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_LONGLONG;
        }
    } else {
        if (midint_is_ulteq_imm(val, midtype_longlong_smax)) {
            return NUMLIT_LONGLONG;
        } else if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type_ulonglong(const struct mid_APInt *val,
                                                 int base,
                                                 struct mid_Position pos,
                                                 const char *line,
                                                 struct mid_DiagVec *diags)
{
    if (base == 10) {
        if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    } else {
        if (midint_is_ulteq_imm(val, midtype_longlong_umax)) {
            return NUMLIT_ULONGLONG;
        } else {
            midgen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type(const struct mid_APInt *val, int base,
                                       enum NumLitType type,
                                       struct mid_Position pos,
                                       const char *line,
                                       struct mid_DiagVec *diags)
{
    switch (type) {
    case NUMLIT_INT:
        return sel_numlit_type_int(val, base, pos, line, diags);

    case NUMLIT_UINT:
        return sel_numlit_type_uint(val, base, pos, line, diags);

    case NUMLIT_LONG:
        return sel_numlit_type_long(val, base, pos, line, diags);

    case NUMLIT_ULONG:
        return sel_numlit_type_ulong(val, base, pos, line, diags);

    case NUMLIT_LONGLONG:
        return sel_numlit_type_longlong(val, base, pos, line, diags);

    case NUMLIT_ULONGLONG:
        return sel_numlit_type_ulonglong(val, base, pos, line, diags);

    default:
        MID_CRASH("type is not an integer lit");
    }
}

static int32_t numlit_type_size(enum NumLitType type)
{
    switch (type) {
    case NUMLIT_INT:
    case NUMLIT_UINT:
        return midtype_int_size;

    case NUMLIT_LONG:
    case NUMLIT_ULONG:
        return midtype_long_size;

    case NUMLIT_LONGLONG:
    case NUMLIT_ULONGLONG:
        return midtype_longlong_size;

    case NUMLIT_FLOAT:
        return midtype_float_size;

    case NUMLIT_DOUBLE:
        return midtype_double_size;

    case NUMLIT_LONGDOUBLE:
        return midtype_longdouble_size;
    }
}

static bool numlit_type_signed(enum NumLitType type)
{
    return !(type == NUMLIT_UINT || type == NUMLIT_ULONG ||
             type == NUMLIT_ULONGLONG);
}

// end - out variable and can be NULL
static struct NumLit read_numlit(const char *src, mid_isize start,
                                 mid_isize *out_end, struct mid_Position pos,
                                 const char *line, struct mid_DiagVec *diags)
{
    mid_isize digits_end = find_numlit_digits_end(src, start);
    mid_isize lit_end;
    auto is_decimal = numlit_is_decimal(src, start, digits_end);
    auto type = numlit_type(src, digits_end, is_decimal, &lit_end);

    if (out_end)
        *out_end = lit_end;

    struct mid_Dynstr str = midstr_init();
    for (mid_isize i = start; i < lit_end; ++i)
        midstr_append_char(&str, src[i]);

    struct NumLit ret;
    ret.type = type;

    switch (type) {
    case NUMLIT_INT:
    case NUMLIT_LONG:
    case NUMLIT_LONGLONG:
    case NUMLIT_UINT:
    case NUMLIT_ULONG:
    case NUMLIT_ULONGLONG: {
        auto info = midlit_read_intlit(src, start, NULL);
        ret.val.i = info.value;
        ret.type =
            sel_numlit_type(&ret.val.i, info.base, ret.type, pos, line, diags);
        midint_ext(&ret.val.i, numlit_type_size(ret.type) * 8,
                   numlit_type_signed(ret.type));
        break;
    }

    // TODO: add support for parsing arbitrarily large floats as rn this breaks
    //       on floats larger than the implementation's double
    case NUMLIT_FLOAT:
        ret.val.flt = midflt_init(strtod(&src[start], NULL), midtype_float_kind,
                                  midtype_default_rmode);
        break;

    case NUMLIT_DOUBLE:
        ret.val.flt = midflt_init(strtod(&src[start], NULL),
                                  midtype_double_kind, midtype_default_rmode);
        break;

    case NUMLIT_LONGDOUBLE:
        ret.val.flt =
            midflt_init(strtod(&src[start], NULL), midtype_longdouble_kind,
                        midtype_default_rmode);
        break;
    }

    midstr_deinit(&str);

    return ret;
}

static enum midlex_TokenType numlit_type_to_tok_type(enum NumLitType type)
{
    switch (type) {
    case NUMLIT_INT:
        return MIDLEX_TOKENTYPE_INT_LIT;

    case NUMLIT_UINT:
        return MIDLEX_TOKENTYPE_UINT_LIT;

    case NUMLIT_LONG:
        return MIDLEX_TOKENTYPE_LONG_LIT;

    case NUMLIT_ULONG:
        return MIDLEX_TOKENTYPE_ULONG_LIT;

    case NUMLIT_LONGLONG:
        return MIDLEX_TOKENTYPE_LONGLONG_LIT;

    case NUMLIT_ULONGLONG:
        return MIDLEX_TOKENTYPE_ULONGLONG_LIT;

    case NUMLIT_FLOAT:
        return MIDLEX_TOKENTYPE_FLOAT_LIT;

    case NUMLIT_DOUBLE:
        return MIDLEX_TOKENTYPE_DOUBLE_LIT;

    case NUMLIT_LONGDOUBLE:
        return MIDLEX_TOKENTYPE_LONGDOUBLE_LIT;
    }
}

// end - out variable and can be NULL
static struct midlex_Token create_numlit_tok(const char *src, mid_isize start,
                                             mid_isize *out_end,
                                             struct mid_Position pos,
                                             const char *line,
                                             struct mid_DiagVec *diags)
{
    auto info = read_numlit(src, start, out_end, pos, line, diags);

    struct midlex_Token ret;
    ret.pos = pos;
    ret.line = line;
    ret.val = info.val;
    ret.type = numlit_type_to_tok_type(info.type);
    return ret;
}

enum midlit_StringType charlit_type(const char *src, mid_isize start,
                                    mid_isize *prefix_end,
                                    struct mid_Position pos, const char *line,
                                    struct mid_DiagVec *diags)
{
    if (prefix_end)
        *prefix_end = start + 1;

    switch (src[start]) {
    case '\'':
    case '"':
        if (prefix_end)
            *prefix_end = start;
        return MIDLIT_STRINGTYPE_CHAR;

    case 'u':
        if (prefix_end)
            *prefix_end = start + 1;
        return MIDLIT_STRINGTYPE_CHAR16;

    case 'U':
        if (prefix_end)
            *prefix_end = start + 1;
        return MIDLIT_STRINGTYPE_CHAR32;

    case 'L':
        if (prefix_end)
            *prefix_end = start + 1;
        return MIDLIT_STRINGTYPE_WCHAR;

    default:
        midgen_dynpush(diags,
                       ((struct mid_Diag){
                           .pos = pos,
                           .line = line,
                           .msg = midcmd_fmt_to_str(
                               "unknown char literal prefix '%c'", src[start]),
                           .err = MIDDIAG_ERR_BAD_LITERAL,
                           .type = MIDDIAG_TYPE_ERROR,
                       }));
        return MIDLIT_STRINGTYPE_CHAR;
    }
}

static struct mid_Diag expected_tok_err(const char *name,
                                        struct mid_Position pos,
                                        const char *line,
                                        enum middiag_ErrT type)
{
    return (struct mid_Diag){
        .pos = pos,
        .line = line,
        .msg = midcmd_fmt_to_str("expected %s", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static enum midlex_TokenType
charlit_type_to_tok_type(enum midlit_StringType type)
{
    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        return MIDLEX_TOKENTYPE_CHAR_LIT;

    case MIDLIT_STRINGTYPE_WCHAR:
        return MIDLEX_TOKENTYPE_WCHAR_LIT;

    case MIDLIT_STRINGTYPE_CHAR16:
        return MIDLEX_TOKENTYPE_CHAR16_LIT;

    case MIDLIT_STRINGTYPE_CHAR32:
        return MIDLEX_TOKENTYPE_CHAR32_LIT;
    }
}

static enum midlex_TokenType
charlit_type_to_str_tok_type(enum midlit_StringType type)
{
    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        return MIDLEX_TOKENTYPE_STRING_LIT;

    case MIDLIT_STRINGTYPE_WCHAR:
        return MIDLEX_TOKENTYPE_WSTRING_LIT;

    case MIDLIT_STRINGTYPE_CHAR16:
        return MIDLEX_TOKENTYPE_STRING16_LIT;

    case MIDLIT_STRINGTYPE_CHAR32:
        return MIDLEX_TOKENTYPE_STRING32_LIT;
    }
}

bool verify_charlit_value(uint32_t val, enum midlit_StringType type,
                          struct mid_Position pos, const char *line,
                          struct mid_DiagVec *diags)
{
    bool too_big = false;

    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        too_big = val > midtype_char_umax;
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        too_big = val > midtype_wchar_umax;
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        too_big = val > UINT16_MAX;
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        break;
    }

    if (too_big)
        midgen_dynpush(diags,
                       ((struct mid_Diag){
                           .pos = pos,
                           .line = line,
                           .msg = midcmd_fmt_to_str(
                               "character to big to fit in character literal"),
                           .err = MIDDIAG_ERR_BAD_LITERAL,
                           .type = MIDDIAG_TYPE_ERROR,
                       }));

    return !too_big;
}

static struct midlex_Token create_charlit_tok(const char *src, mid_isize start,
                                              mid_isize *out_end,
                                              struct mid_Position pos,
                                              const char *line,
                                              struct mid_DiagVec *diags)
{
    mid_isize lquote;
    auto type = charlit_type(src, start, &lquote, pos, line, diags);
    // control flow shouldn't get here otherwise but better safe than sorry
    assert(src[lquote] == '\'');

    struct midlex_Token ret = {};
    ret.pos = pos;
    ret.line = line;
    ret.type = charlit_type_to_tok_type(type);
    mid_isize rquote;
    auto c = midutf8_read_char(src, lquote + 1, &rquote);
    ret.val.i = midint_init(midlit_strtype_char_size(type) * 8, c, false);

    if (!verify_charlit_value(midint_to_uint(&ret.val.i), type, pos, line,
                              diags))
        midint_assign_uimm(&ret.val.i, 0);

    if (src[rquote] != '\'') {
        midgen_dynpush(
            diags, expected_tok_err("'", pos, line, MIDDIAG_ERR_MISSING_QUOTE));
        if (out_end)
            *out_end = rquote;
    } else if (out_end) {
        *out_end = rquote + 1;
    }

    return ret;
}

void realloc_strlit(struct midlit_String *str, mid_isize cap)
{
    switch (str->type) {
    case MIDLIT_STRINGTYPE_CHAR:
        str->c = mid_realloc(str->c, cap * sizeof(*str->c));
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        str->wc = mid_realloc(str->wc, cap * sizeof(*str->wc));
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        str->c16 = mid_realloc(str->c16, cap * sizeof(*str->c16));
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        str->c32 = mid_realloc(str->c32, cap * sizeof(*str->c32));
        break;
    }
}

static void strlit_add(struct midlit_String *str, mid_isize idx, uint32_t c)
{
    switch (str->type) {
    case MIDLIT_STRINGTYPE_CHAR:
        str->c[idx] = c;
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        str->wc[idx] = c;
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        str->c16[idx] = c;
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        str->c32[idx] = c;
        break;
    }
}

struct midlit_String read_strlit(const char *src, mid_isize lquote,
                                 mid_isize *out_end,
                                 enum midlit_StringType type,
                                 struct mid_Position pos, const char *line,
                                 struct mid_DiagVec *diags)
{
    mid_isize len = 0;
    mid_isize cap = 128;
    struct midlit_String str = {.type = type};
    realloc_strlit(&str, cap);

    mid_isize i;
    for (i = lquote + 1; src[i] != '"' && src[i] != '\n';) {
        uint32_t c = midutf8_read_char(src, i, &i);
        verify_charlit_value(c, type, pos, line, diags);

        strlit_add(&str, len++, c);
        if (len == cap)
            realloc_strlit(&str, cap += 128);
    }

    strlit_add(&str, len, '\0');

    if (src[i] != '"')
        midgen_dynpush(
            diags, expected_tok_err("\"", pos, line, MIDDIAG_ERR_BAD_LITERAL));

    if (out_end)
        *out_end = i + (src[i] == '"');
    return str;
}

static struct midlex_Token
create_strlit_tok(const char *src, struct midlit_StringVec *str_lits,
                  mid_isize start, mid_isize *out_end, struct mid_Position pos,
                  const char *line, struct mid_DiagVec *diags)
{
    mid_isize lquote;
    auto type = charlit_type(src, start, &lquote, pos, line, diags);
    assert(src[lquote] == '"');

    struct midlex_Token ret = {};
    ret.pos = pos;
    ret.line = line;
    ret.type = charlit_type_to_str_tok_type(type);

    auto lit = read_strlit(src, lquote, out_end, type, pos, line, diags);
    midgen_dynpush(str_lits, lit);
    ret.val.str = lit;

    return ret;
}

static bool is_identifier_char(char c)
{
    return isalnum(c) || c == '_';
}

static mid_isize identifier_end(const char *src, mid_isize start)
{
    mid_isize end;
    for (end = start; is_identifier_char(src[end]); ++end)
        ;
    return end;
}

// end - an out variable and can be NULL
static char *read_identifier(const char *src, mid_isize start, mid_isize *end)
{
    mid_isize id_end = identifier_end(src, start);
    if (end)
        *end = id_end;

    mid_isize len = id_end - start;
    char *str = mid_malloc((len + 1) * sizeof(*str));
    str[len] = '\0';

    for (mid_isize i = 0; i < len; ++i)
        str[i] = src[i + start];

    return str;
}

static struct midlex_Token
create_identifier_tok(char *id, struct mid_Position pos, const char *line)
{
    if (!strcmp(id, "void"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_VOID};
    else if (!strcmp(id, "char"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CHAR};
    else if (!strcmp(id, "wchar_t"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_WCHAR};
    else if (!strcmp(id, "char16_t"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CHAR16};
    else if (!strcmp(id, "char32_t"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CHAR32};
    else if (!strcmp(id, "bool"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_BOOL};
    else if (!strcmp(id, "int"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_INT};
    else if (!strcmp(id, "float"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_FLOAT};
    else if (!strcmp(id, "double"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_DOUBLE};
    else if (!strcmp(id, "class"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CLASS};
    else if (!strcmp(id, "struct"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_STRUCT};
    else if (!strcmp(id, "union"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_UNION};
    else if (!strcmp(id, "enum"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_ENUM};
    else if (!strcmp(id, "auto"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_AUTO};

    else if (!strcmp(id, "short"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_SHORT};
    else if (!strcmp(id, "long"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_LONG};
    else if (!strcmp(id, "signed"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_SIGNED};
    else if (!strcmp(id, "unsigned"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_UNSIGNED};
    else if (!strcmp(id, "static"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_STATIC};
    else if (!strcmp(id, "constexpr"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CONSTEXPR};
    else if (!strcmp(id, "typedef"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_TYPEDEF};

    else if (!strcmp(id, "const"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CONST};
    else if (!strcmp(id, "volatile"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_VOLATILE};

    else if (!strcmp(id, "typeid"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_TYPEID};

    else if (!strcmp(id, "const_cast"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_CONSTCAST};
    else if (!strcmp(id, "dynamic_cast"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_DYNAMICCAST};
    else if (!strcmp(id, "reinterpret_cast"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_REINTERPRETCAST};
    else if (!strcmp(id, "static_cast"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_STATICCAST};

    else if (!strcmp(id, "new"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_NEW};
    else if (!strcmp(id, "delete"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_DELETE};

    else if (!strcmp(id, "throw"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_THROW};

    else if (!strcmp(id, "true"))
        return (struct midlex_Token){.pos = pos,
                                     .line = line,
                                     .type = MIDLEX_TOKENTYPE_BOOL_LIT,
                                     .val.i =
                                         midint_one(midtype_bool_size * 8)};
    else if (!strcmp(id, "false"))
        return (struct midlex_Token){.pos = pos,
                                     .line = line,
                                     .type = MIDLEX_TOKENTYPE_BOOL_LIT,
                                     .val.i =
                                         midint_zero(midtype_bool_size * 8)};
    else if (!strcmp(id, "nullptr"))
        return (struct midlex_Token){.pos = pos,
                                     .line = line,
                                     .type = MIDLEX_TOKENTYPE_NULLPTR_LIT,
                                     .val.i =
                                         midint_zero(midtype_ptr_size * 8)};

    else if (!strcmp(id, "public"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_PUBLIC};
    else if (!strcmp(id, "private"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_PRIVATE};
    else if (!strcmp(id, "protected"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_PROTECTED};

    else if (!strcmp(id, "this"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_THIS};

    else if (!strcmp(id, "namespace"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_NAMESPACE};

    else if (!strcmp(id, "return"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_RETURN};

    else if (!strcmp(id, "noexcept"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_NOEXCEPT};
    else if (!strcmp(id, "final"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_FINAL};
    else if (!strcmp(id, "override"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_OVERRIDE};

    else if (!strcmp(id, "default"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_DEFAULT};

    else if (!strcmp(id, "template"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_TEMPLATE};
    else if (!strcmp(id, "typename"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_TYPENAME};

    else if (!strcmp(id, "sizeof"))
        return (struct midlex_Token){
            .pos = pos, .line = line, .type = MIDLEX_TOKENTYPE_SIZEOF};

    else
        return (struct midlex_Token){.pos = pos,
                                     .line = line,
                                     .type = MIDLEX_TOKENTYPE_IDENTIFIER,
                                     .ident = id};
}

static mid_isize skip_to_line_end(const char *src, mid_isize start,
                                  struct mid_Position *pos)
{
    mid_isize i = start;
    while (src[++i] != '\n')
        ++pos->column;
    return i - 1;
}

// c style comment: /* ... */
//                  ^
//                start
//                          ^
//                        return
static mid_isize skip_c_comment(const char *src, mid_isize start,
                                struct mid_Position *pos)
{
    mid_isize i = start + 2;

    while (src[i] != '*' || src[i + 1] != '/') {
        if (src[i] == '\n') {
            ++pos->line;
            pos->column = 1;
        }

        ++i, ++pos->column;
    }

    return i + 1;
}

// very ugly function
static char *symb_in_tbl(struct midsymb_Table *tbl, const char *symb)
{
    for (mid_isize i = 0; i < tbl->len; ++i) {
        if (!strcmp(tbl->arr[i], symb))
            return tbl->arr[i];
    }

    return NULL;
}

static struct midlex_Tokenize read_tokens(const char *src, const char *file)
{
    struct midlex_TokenVec toks = {};
    struct midsymb_Table symbtbl = {};
    struct midlit_StringVec str_lits = {};
    struct mid_DiagVec diags = {};
    struct mid_Position pos = {.file = file, .line = 1, .column = 1};

    const char *line_start = src;

    for (mid_isize i = 0; src[i] != '\0'; ++i, ++pos.column) {
        if (src[i] == '/' && src[i + 1] == '/') {
            i = skip_to_line_end(src, i, &pos);
            continue;
        } else if (src[i] == '/' && src[i + 1] == '*') {
            i = skip_c_comment(src, i, &pos);
            continue;
        }

        switch (src[i]) {
        case '\n':
            line_start = &src[i + 1];
            ++pos.line;
            pos.column = 0; // gets inc'd later
            break;

        // whitespace
        case ' ':
        case '\t':
        case '\v':
        case '\f':
        case '\r':
            break;

        case ':':
            if (src[i + 1] == ':') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_SCOPE_RES, pos,
                                                line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_COLON,
                                                       pos, line_start));
            }
            break;

        case '.':
            if (isdigit(src[i + 1])) { // literals like .5
                auto old_i = i;
                midgen_dynpush(&toks, create_numlit_tok(src, i, &i, pos,
                                                        line_start, &diags));
                --i;
                pos.column += i - old_i;
            } else if (src[i + 1] == '*') {
                midgen_dynpush(
                    &toks, create_basic_tok(MIDLEX_TOKENTYPE_PTR_TO_MEMB_SEL,
                                            pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '.' && src[i + 2] == '.') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_ELLIPSIS, pos,
                                                line_start));
                i += 2;
                pos.column += 2;
            } else {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_MEMB_SEL, pos,
                                                line_start));
            }
            break;

        case '*':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_MUL_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_MUL,
                                                       pos, line_start));
            }
            break;

        case '/':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_DIV_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_DIV,
                                                       pos, line_start));
            }
            break;

        case '%':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_MOD_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_MOD,
                                                       pos, line_start));
            }
            break;

        case '+':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_ADD_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '+') {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_INC,
                                                       pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_ADD,
                                                       pos, line_start));
            }
            break;

        case '-':
            if (src[i + 1] == '>') {
                if (src[i + 2] == '*')
                    midgen_dynpush(
                        &toks,
                        create_basic_tok(MIDLEX_TOKENTYPE_PTR_TO_PTR_MEMB_SEL,
                                         pos, line_start));
                else
                    midgen_dynpush(
                        &toks, create_basic_tok(MIDLEX_TOKENTYPE_PTR_MEMB_SEL,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_SUB_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '-') {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_DEC,
                                                       pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_SUB,
                                                       pos, line_start));
            }
            break;

        case '<':
            if (src[i + 1] == '<') {
                if (src[i + 1] == '=')
                    midgen_dynpush(
                        &toks,
                        create_basic_tok(MIDLEX_TOKENTYPE_LEFT_SHIFT_ASSIGN,
                                         pos, line_start));
                else
                    midgen_dynpush(&toks,
                                   create_basic_tok(MIDLEX_TOKENTYPE_LEFT_SHIFT,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_LTEQ,
                                                       pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_LT, pos,
                                                       line_start));
            }
            break;

        case '>':
            if (src[i + 1] == '>') {
                if (src[i + 1] == '=')
                    midgen_dynpush(
                        &toks,
                        create_basic_tok(MIDLEX_TOKENTYPE_RIGHT_SHIFT_ASSIGN,
                                         pos, line_start));
                else
                    midgen_dynpush(
                        &toks, create_basic_tok(MIDLEX_TOKENTYPE_RIGHT_SHIFT,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_GTEQ,
                                                       pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_GT, pos,
                                                       line_start));
            }
            break;

        case '=':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_EQ, pos,
                                                       line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_ASSIGN,
                                                       pos, line_start));
            }
            break;

        case '!':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_NEQ,
                                                       pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_LOGICAL_NOT,
                                                pos, line_start));
            }
            break;

        case '&':
            if (src[i + 1] == '&') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_LOGICAL_AND,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_AND_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_BITWISE_AND,
                                                pos, line_start));
            }
            break;

        case '^':
            if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_XOR_ASSIGN,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_BITWISE_XOR,
                                                pos, line_start));
            }
            break;

        case '|':
            if (src[i + 1] == '|') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_LOGICAL_OR,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_OR_ASSIGN, pos,
                                                line_start));
                ++i;
                ++pos.column;
            } else {
                midgen_dynpush(&toks,
                               create_basic_tok(MIDLEX_TOKENTYPE_BITWISE_OR,
                                                pos, line_start));
            }
            break;

        case ',':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_COMMA, pos,
                                                   line_start));
            break;

        case '~':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_BITWISE_NOT,
                                                   pos, line_start));
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            auto old_i = i;
            midgen_dynpush(
                &toks, create_numlit_tok(src, i, &i, pos, line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        case '(':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_L_PAREN,
                                                   pos, line_start));
            break;
        case ')':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_R_PAREN,
                                                   pos, line_start));
            break;
        case '[':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_L_SQBRACKET,
                                                   pos, line_start));
            break;
        case ']':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_R_SQBRACKET,
                                                   pos, line_start));
            break;
        case '{':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_L_CURLY,
                                                   pos, line_start));
            break;
        case '}':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_R_CURLY,
                                                   pos, line_start));
            break;

        case ';':
            midgen_dynpush(&toks, create_basic_tok(MIDLEX_TOKENTYPE_SEMICOLON,
                                                   pos, line_start));
            break;

        // ew
        CASE_ISALPHA:
        case '_': {
            if (src[i + 1] == '\'') {
                goto parse_char_lit;
            } else if (src[i + 1] == '"') {
                goto parse_str_lit;
            } else {
                auto old_i = i;
                char *id = read_identifier(src, i, &i);
                --i;
                char *in_tbl = symb_in_tbl(&symbtbl, id);
                if (in_tbl) {
                    free(id);
                    id = in_tbl;
                }
                auto tok = create_identifier_tok(id, pos, line_start);
                midgen_dynpush(&toks, tok);
                // if the symbol is an actual identifier then it needs to be
                // added to the symbol table, otherwise the identifier can be
                // discarded
                if (!in_tbl && tok.type == MIDLEX_TOKENTYPE_IDENTIFIER)
                    midgen_dynpush(&symbtbl, id);
                else if (!in_tbl)
                    free(id);
                pos.column += i - old_i;
                break;
            }
        }

        case '\'': {
        parse_char_lit:
            auto old_i = i;
            midgen_dynpush(
                &toks, create_charlit_tok(src, i, &i, pos, line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        case '"': {
        parse_str_lit:
            auto old_i = i;
            midgen_dynpush(&toks, create_strlit_tok(src, &str_lits, i, &i, pos,
                                                    line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        default: {
            // column isn't updated cuz it's still one character
            char *c = midutf8_char_to_str(midutf8_read_char(src, i, &i));
            --i;
            struct mid_Diag err = {
                .type = MIDDIAG_TYPE_ERROR,
                .err = MIDDIAG_ERR_UNKNOWN_SYMBOL,
                .msg = midcmd_fmt_to_str("unknown symbol '%s'", c),
                .pos = pos,
                .line = line_start};
            midgen_dynpush(&diags, err);
            free(c);
            break;
        }
        }
    }
    midgen_dynpush(&toks,
                   create_basic_tok(MIDLEX_TOKENTYPE_END, pos, line_start));

    struct midlex_Tokenize ret;
    ret.toks = toks;
    ret.symtbl = symbtbl;
    ret.str_lits = str_lits;
    ret.diags = diags;
    return ret;
}

struct midlex_Tokenize midlex_tokenize(const char *src, const char *file)
{
    auto lex = read_tokens(src, file);

    return lex;
}

void midlex_Tokenize_deinit(struct midlex_Tokenize *self)
{
    midgen_dyndeinit(&self->toks, midlex_Token_deinit);
    midgen_dyndeinit(&self->symtbl, midsymb_deinit_symbol);
    midgen_dyndeinit(&self->str_lits, midlit_String_deinit);
    midgen_dyndeinit(&self->diags, middiag_deinit);
}
