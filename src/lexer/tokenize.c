#include "tokenize.h"
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
#include "print.h"
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

static struct MidLexer_Token create_basic_tok(enum MidLexer_TokenType type,
                                              struct Mid_Position pos,
                                              const char *line)
{
    return (struct MidLexer_Token){.type = type, .pos = pos, .line = line};
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
    NUMMIDLIT_INT,
    NUMMIDLIT_UINT,
    NUMMIDLIT_LONG,
    NUMMIDLIT_ULONG,
    NUMMIDLIT_LONGLONG,
    NUMMIDLIT_ULONGLONG,
    NUMMIDLIT_FLOAT,
    NUMMIDLIT_DOUBLE,
    NUMMIDLIT_LONGDOUBLE,
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
                return NUMMIDLIT_ULONGLONG;
            }
            if (suffix_end)
                *suffix_end = end + 2;
            return NUMMIDLIT_ULONG;
        }
        if (suffix_end)
            *suffix_end = end + 1;
        return NUMMIDLIT_UINT;
    } else if (c0 == 'l') {
        if (c1 == 'l') {
            if (suffix_end)
                *suffix_end = end + 2;
            return NUMMIDLIT_LONGLONG;
        }
        if (suffix_end)
            *suffix_end = end + 1;
        return is_decimal ? NUMMIDLIT_LONGDOUBLE : NUMMIDLIT_LONG;
    } else if (c0 == 'f') {
        if (suffix_end)
            *suffix_end = end + 1;
        return NUMMIDLIT_FLOAT;
    } else {
        if (suffix_end)
            *suffix_end = end;
        return is_decimal ? NUMMIDLIT_DOUBLE : NUMMIDLIT_INT;
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
    union MidLit_Value val;
    enum NumLitType type;
};

static struct MidDiag_Diag intlit_too_big_err(struct Mid_Position pos,
                                              const char *line)
{
    return (struct MidDiag_Diag){
        .pos = pos,
        .line = line,
        .msg = MidPrint_fmt_to_str("integer literal too big"),
        .err = MIDDIAG_ERR_BAD_LITERAL,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static enum NumLitType
sel_numlit_type_int(u64 val, int base, struct Mid_Position pos,
                    const char *line, struct MidDiag_DiagVec *diags)
{
    if (base == 10) {
        if (val <= MidTypes_int_smax) {
            return NUMMIDLIT_INT;
        } else if (val <= MidTypes_long_smax) {
            return NUMMIDLIT_LONG;
        } else if (val <= MidTypes_longlong_smax) {
            return NUMMIDLIT_LONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_LONGLONG;
        }
    } else {
        if (val <= MidTypes_int_smax) {
            return NUMMIDLIT_INT;
        } else if (val <= MidTypes_int_umax) {
            return NUMMIDLIT_UINT;
        } else if (val <= MidTypes_long_smax) {
            return NUMMIDLIT_LONG;
        } else if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_smax) {
            return NUMMIDLIT_LONGLONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    }
}

static enum NumLitType
sel_numlit_type_uint(u64 val, int base, struct Mid_Position pos,
                     const char *line, struct MidDiag_DiagVec *diags)
{
    if (base == 10) {
        if (val <= MidTypes_int_umax) {
            return NUMMIDLIT_UINT;
        } else if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    } else {
        if (val <= MidTypes_int_umax) {
            return NUMMIDLIT_UINT;
        } else if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    }
}

static enum NumLitType
sel_numlit_type_long(u64 val, int base, struct Mid_Position pos,
                     const char *line, struct MidDiag_DiagVec *diags)
{
    if (base == 10) {
        if (val <= MidTypes_long_smax) {
            return NUMMIDLIT_LONG;
        } else if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_smax) {
            return NUMMIDLIT_LONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_LONGLONG;
        }
    } else {
        if (val <= MidTypes_long_smax) {
            return NUMMIDLIT_LONG;
        } else if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_smax) {
            return NUMMIDLIT_LONGLONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    }
}

static enum NumLitType
sel_numlit_type_ulong(u64 val, int base, struct Mid_Position pos,
                      const char *line, struct MidDiag_DiagVec *diags)
{
    if (base == 10) {
        if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    } else {
        if (val <= MidTypes_long_umax) {
            return NUMMIDLIT_ULONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    }
}

static enum NumLitType
sel_numlit_type_longlong(u64 val, int base, struct Mid_Position pos,
                         const char *line,
                         struct MidDiag_DiagVec *diags)
{
    if (base == 10) {
        if (val <= MidTypes_longlong_smax) {
            return NUMMIDLIT_LONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_LONGLONG;
        }
    } else {
        if (val <= MidTypes_longlong_smax) {
            return NUMMIDLIT_LONGLONG;
        } else if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    }
}

static enum NumLitType
sel_numlit_type_ulonglong(u64 val, int base, struct Mid_Position pos,
                          const char *line,
                          struct MidDiag_DiagVec *diags)
{
    if (base == 10) {
        if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    } else {
        if (val <= MidTypes_longlong_umax) {
            return NUMMIDLIT_ULONGLONG;
        } else {
            MidGen_dynpush(diags, intlit_too_big_err(pos, line));
            return NUMMIDLIT_ULONGLONG;
        }
    }
}

static enum NumLitType sel_numlit_type(u64 val, int base, enum NumLitType type,
                                       struct Mid_Position pos,
                                       const char *line,
                                       struct MidDiag_DiagVec *diags)
{
    switch (type) {
    case NUMMIDLIT_INT:
        return sel_numlit_type_int(val, base, pos, line, diags);

    case NUMMIDLIT_UINT:
        return sel_numlit_type_uint(val, base, pos, line, diags);

    case NUMMIDLIT_LONG:
        return sel_numlit_type_long(val, base, pos, line, diags);

    case NUMMIDLIT_ULONG:
        return sel_numlit_type_ulong(val, base, pos, line, diags);

    case NUMMIDLIT_LONGLONG:
        return sel_numlit_type_longlong(val, base, pos, line, diags);

    case NUMMIDLIT_ULONGLONG:
        return sel_numlit_type_ulonglong(val, base, pos, line, diags);

    default:
        MID_CRASH("type is not an integer lit");
    }
}

// end - out variable and can be NULL
static struct NumLit read_numlit(const char *src, mid_isize start,
                                 mid_isize *out_end, struct Mid_Position pos,
                                 const char *line,
                                 struct MidDiag_DiagVec *diags)
{
    mid_isize digits_end = find_numlit_digits_end(src, start);
    mid_isize lit_end;
    auto is_decimal = numlit_is_decimal(src, start, digits_end);
    auto type = numlit_type(src, digits_end, is_decimal, &lit_end);

    if (out_end)
        *out_end = lit_end;

    struct Mid_Dynstr str = MidDynstr_init();
    for (mid_isize i = start; i < lit_end; ++i)
        MidDynstr_append_char(&str, src[i]);

    struct NumLit ret;
    ret.type = type;

    switch (type) {
    case NUMMIDLIT_INT:
    case NUMMIDLIT_LONG:
    case NUMMIDLIT_LONGLONG:
    case NUMMIDLIT_UINT:
    case NUMMIDLIT_ULONG:
    case NUMMIDLIT_ULONGLONG:
        auto info = MidLit_read_intlit(src, start, NULL);
        ret.val.uint = info.value;
        ret.type = sel_numlit_type(ret.val.uint, info.base, ret.type, pos, line,
                                   diags);
        break;

    case NUMMIDLIT_FLOAT:
    case NUMMIDLIT_DOUBLE:
    case NUMMIDLIT_LONGDOUBLE:
        ret.val.flt = strtold(&src[start], NULL);
        break;
    }

    MidDynstr_deinit(&str);

    return ret;
}

static enum MidLexer_TokenType numlit_type_to_tok_type(enum NumLitType type)
{
    switch (type) {
    case NUMMIDLIT_INT:
        return MIDLEXER_TOKENTYPE_INT_LIT;

    case NUMMIDLIT_UINT:
        return MIDLEXER_TOKENTYPE_UINT_LIT;

    case NUMMIDLIT_LONG:
        return MIDLEXER_TOKENTYPE_LONG_LIT;

    case NUMMIDLIT_ULONG:
        return MIDLEXER_TOKENTYPE_ULONG_LIT;

    case NUMMIDLIT_LONGLONG:
        return MIDLEXER_TOKENTYPE_LONGLONG_LIT;

    case NUMMIDLIT_ULONGLONG:
        return MIDLEXER_TOKENTYPE_ULONGLONG_LIT;

    case NUMMIDLIT_FLOAT:
        return MIDLEXER_TOKENTYPE_FLOAT_LIT;

    case NUMMIDLIT_DOUBLE:
        return MIDLEXER_TOKENTYPE_DOUBLE_LIT;

    case NUMMIDLIT_LONGDOUBLE:
        return MIDLEXER_TOKENTYPE_LONGDOUBLE_LIT;
    }
}

// end - out variable and can be NULL
static struct MidLexer_Token
create_numlit_tok(const char *src, mid_isize start, mid_isize *out_end,
                  struct Mid_Position pos, const char *line,
                  struct MidDiag_DiagVec *diags)
{
    auto info = read_numlit(src, start, out_end, pos, line, diags);

    struct MidLexer_Token ret;
    ret.pos = pos;
    ret.line = line;
    ret.val = info.val;
    ret.type = numlit_type_to_tok_type(info.type);
    return ret;
}

enum MidLit_StringType charlit_type(const char *src, mid_isize start,
                                    mid_isize *prefix_end,
                                    struct Mid_Position pos, const char *line,
                                    struct MidDiag_DiagVec *diags)
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
        MidGen_dynpush(diags,
                    ((struct MidDiag_Diag){
                        .pos = pos,
                        .line = line,
                        .msg = MidPrint_fmt_to_str(
                            "unknown char literal prefix '%c'", src[start]),
                        .err = MIDDIAG_ERR_BAD_LITERAL,
                        .type = MIDDIAG_TYPE_ERROR,
                    }));
        return MIDLIT_STRINGTYPE_CHAR;
    }
}

static struct MidDiag_Diag expected_tok_err(const char *name,
                                            struct Mid_Position pos,
                                            const char *line,
                                            enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = pos,
        .line = line,
        .msg = MidPrint_fmt_to_str("expected %s", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static enum MidLexer_TokenType
charlit_type_to_tok_type(enum MidLit_StringType type)
{
    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        return MIDLEXER_TOKENTYPE_CHAR_LIT;

    case MIDLIT_STRINGTYPE_WCHAR:
        return MIDLEXER_TOKENTYPE_WCHAR_LIT;

    case MIDLIT_STRINGTYPE_CHAR16:
        return MIDLEXER_TOKENTYPE_CHAR16_LIT;

    case MIDLIT_STRINGTYPE_CHAR32:
        return MIDLEXER_TOKENTYPE_CHAR32_LIT;
    }
}

static enum MidLexer_TokenType
charlit_type_to_str_tok_type(enum MidLit_StringType type)
{
    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        return MIDLEXER_TOKENTYPE_STRING_LIT;

    case MIDLIT_STRINGTYPE_WCHAR:
        return MIDLEXER_TOKENTYPE_WSTRING_LIT;

    case MIDLIT_STRINGTYPE_CHAR16:
        return MIDLEXER_TOKENTYPE_STRING16_LIT;

    case MIDLIT_STRINGTYPE_CHAR32:
        return MIDLEXER_TOKENTYPE_STRING32_LIT;
    }
}

bool verify_charlit_value(u32 val, enum MidLit_StringType type,
                          struct Mid_Position pos, const char *line,
                          struct MidDiag_DiagVec *diags)
{
    bool too_big = false;

    switch (type) {
    case MIDLIT_STRINGTYPE_CHAR:
        too_big = val > MidTypes_char_umax;
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        too_big = val > MidTypes_wchar_umax;
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        too_big = val > UINT16_MAX;
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        // val is exactly 32 bits
        break;
    }

    if (too_big)
        MidGen_dynpush(diags,
                    ((struct MidDiag_Diag){
                        .pos = pos,
                        .line = line,
                        .msg = MidPrint_fmt_to_str(
                            "character to big to fit in character literal"),
                        .err = MIDDIAG_ERR_BAD_LITERAL,
                        .type = MIDDIAG_TYPE_ERROR,
                    }));

    return !too_big;
}

static struct MidLexer_Token
create_charlit_tok(const char *src, mid_isize start, mid_isize *out_end,
                   struct Mid_Position pos, const char *line,
                   struct MidDiag_DiagVec *diags)
{
    mid_isize lquote;
    auto type = charlit_type(src, start, &lquote, pos, line, diags);
    // control flow shouldn't get here otherwise but better safe than sorry
    assert(src[lquote] == '\'');

    struct MidLexer_Token ret = {};
    ret.pos = pos;
    ret.line = line;
    ret.type = charlit_type_to_tok_type(type);
    mid_isize rquote;
    ret.val.uint = MidUTF8_read_char(src, lquote + 1, &rquote);

    if (!verify_charlit_value(ret.val.uint, type, pos, line, diags))
        ret.val.uint = '\0';

    if (src[rquote] != '\'') {
        MidGen_dynpush(
            diags, expected_tok_err("'", pos, line, MIDDIAG_ERR_MISSING_QUOTE));
        if (out_end)
            *out_end = rquote;
    } else if (out_end) {
        *out_end = rquote + 1;
    }

    return ret;
}

void realloc_strlit(struct MidLit_String *str, mid_isize cap)
{
    switch (str->type) {
    case MIDLIT_STRINGTYPE_CHAR:
        str->c = Mid_realloc(str->c, cap * sizeof(*str->c));
        break;

    case MIDLIT_STRINGTYPE_WCHAR:
        str->wc = Mid_realloc(str->wc, cap * sizeof(*str->wc));
        break;

    case MIDLIT_STRINGTYPE_CHAR16:
        str->c16 = Mid_realloc(str->c16, cap * sizeof(*str->c16));
        break;

    case MIDLIT_STRINGTYPE_CHAR32:
        str->c32 = Mid_realloc(str->c32, cap * sizeof(*str->c32));
        break;
    }
}

static void strlit_add(struct MidLit_String *str, mid_isize idx, u32 c)
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

struct MidLit_String read_strlit(const char *src, mid_isize lquote,
                                 mid_isize *out_end, enum MidLit_StringType type,
                                 struct Mid_Position pos, const char *line,
                                 struct MidDiag_DiagVec *diags)
{
    mid_isize len = 0;
    mid_isize cap = 128;
    struct MidLit_String str = {.type = type};
    realloc_strlit(&str, cap);

    mid_isize i;
    for (i = lquote + 1; src[i] != '"' && src[i] != '\n';) {
        u32 c = MidUTF8_read_char(src, i, &i);
        verify_charlit_value(c, type, pos, line, diags);

        strlit_add(&str, len++, c);
        if (len == cap)
            realloc_strlit(&str, cap += 128);
    }

    strlit_add(&str, len, '\0');

    if (src[i] != '"')
        MidGen_dynpush(diags,
                    expected_tok_err("\"", pos, line, MIDDIAG_ERR_BAD_LITERAL));

    if (out_end)
        *out_end = i + (src[i] == '"');
    return str;
}

static struct MidLexer_Token
create_strlit_tok(const char *src, struct MidLit_StringVec *str_lits,
                  mid_isize start, mid_isize *out_end, struct Mid_Position pos,
                  const char *line, struct MidDiag_DiagVec *diags)
{
    mid_isize lquote;
    auto type = charlit_type(src, start, &lquote, pos, line, diags);
    assert(src[lquote] == '"');

    struct MidLexer_Token ret = {};
    ret.pos = pos;
    ret.line = line;
    ret.type = charlit_type_to_str_tok_type(type);

    auto lit = read_strlit(src, lquote, out_end, type, pos, line, diags);
    MidGen_dynpush(str_lits, lit);
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
    char *str = Mid_malloc((len + 1) * sizeof(*str));
    str[len] = '\0';

    for (mid_isize i = 0; i < len; ++i)
        str[i] = src[i + start];

    return str;
}

static struct MidLexer_Token
create_identifier_tok(char *id, struct Mid_Position pos, const char *line)
{
    if (!strcmp(id, "void"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_VOID};
    else if (!strcmp(id, "char"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CHAR};
    else if (!strcmp(id, "wchar_t"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_WCHAR};
    else if (!strcmp(id, "char16_t"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CHAR16};
    else if (!strcmp(id, "char32_t"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CHAR32};
    else if (!strcmp(id, "bool"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_BOOL};
    else if (!strcmp(id, "int"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_INT};
    else if (!strcmp(id, "float"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_FLOAT};
    else if (!strcmp(id, "double"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_DOUBLE};
    else if (!strcmp(id, "class"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CLASS};
    else if (!strcmp(id, "struct"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_STRUCT};
    else if (!strcmp(id, "union"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_UNION};
    else if (!strcmp(id, "enum"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_ENUM};
    else if (!strcmp(id, "auto"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_AUTO};

    else if (!strcmp(id, "short"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_SHORT};
    else if (!strcmp(id, "long"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_LONG};
    else if (!strcmp(id, "signed"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_SIGNED};
    else if (!strcmp(id, "unsigned"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_UNSIGNED};
    else if (!strcmp(id, "static"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_STATIC};
    else if (!strcmp(id, "constexpr"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CONSTEXPR};
    else if (!strcmp(id, "typedef"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_TYPEDEF};

    else if (!strcmp(id, "const"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CONST};
    else if (!strcmp(id, "volatile"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_VOLATILE};

    else if (!strcmp(id, "typeid"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_TYPEID};

    else if (!strcmp(id, "const_cast"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_CONSTCAST};
    else if (!strcmp(id, "dynamic_cast"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_DYNAMICCAST};
    else if (!strcmp(id, "reinterpret_cast"))
        return (struct MidLexer_Token){.pos = pos,
                                       .line = line,
                                       .type =
                                           MIDLEXER_TOKENTYPE_REINTERPRETCAST};
    else if (!strcmp(id, "static_cast"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_STATICCAST};

    else if (!strcmp(id, "new"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_NEW};
    else if (!strcmp(id, "delete"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_DELETE};

    else if (!strcmp(id, "throw"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_THROW};

    else if (!strcmp(id, "true"))
        return (struct MidLexer_Token){.pos = pos,
                                       .line = line,
                                       .type = MIDLEXER_TOKENTYPE_BOOL_LIT,
                                       .val.sint = true};
    else if (!strcmp(id, "false"))
        return (struct MidLexer_Token){.pos = pos,
                                       .line = line,
                                       .type = MIDLEXER_TOKENTYPE_BOOL_LIT,
                                       .val.sint = false};
    else if (!strcmp(id, "nullptr"))
        return (struct MidLexer_Token){.pos = pos,
                                       .line = line,
                                       .type = MIDLEXER_TOKENTYPE_NULLPTR_LIT,
                                       .val.uint = 0};

    else if (!strcmp(id, "public"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_PUBLIC};
    else if (!strcmp(id, "private"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_PRIVATE};
    else if (!strcmp(id, "protected"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_PROTECTED};

    else if (!strcmp(id, "this"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_THIS};

    else if (!strcmp(id, "namespace"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_NAMESPACE};

    else if (!strcmp(id, "return"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_RETURN};

    else if (!strcmp(id, "noexcept"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_NOEXCEPT};
    else if (!strcmp(id, "final"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_FINAL};
    else if (!strcmp(id, "override"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_OVERRIDE};

    else if (!strcmp(id, "default"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_DEFAULT};

    else if (!strcmp(id, "template"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_TEMPLATE};
    else if (!strcmp(id, "typename"))
        return (struct MidLexer_Token){
            .pos = pos, .line = line, .type = MIDLEXER_TOKENTYPE_TYPENAME};

    else
        return (struct MidLexer_Token){.pos = pos,
                                       .line = line,
                                       .type = MIDLEXER_TOKENTYPE_IDENTIFIER,
                                       .ident = id};
}

static mid_isize skip_to_line_end(const char *src, mid_isize start,
                                struct Mid_Position *pos)
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
                              struct Mid_Position *pos)
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
static char *symb_in_tbl(struct MidSymbol_Table *tbl, const char *symb)
{
    for (mid_isize i = 0; i < tbl->len; ++i) {
        if (!strcmp(tbl->arr[i], symb))
            return tbl->arr[i];
    }

    return NULL;
}

static struct MidLexer_Tokenize read_tokens(const char *src, const char *file)
{
    struct MidLexer_TokenVec toks = {};
    struct MidSymbol_Table symbtbl = {};
    struct MidLit_StringVec str_lits = {};
    struct MidDiag_DiagVec diags = {};
    struct Mid_Position pos = {.file = file, .line = 1, .column = 1};

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
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_SCOPE_RES, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_COLON,
                                                    pos, line_start));
            }
            break;

        case '.':
            if (isdigit(src[i + 1])) { // literals like .5
                auto old_i = i;
                MidGen_dynpush(&toks, create_numlit_tok(src, i, &i, pos,
                                                     line_start, &diags));
                --i;
                pos.column += i - old_i;
            } else if (src[i + 1] == '*') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_PTR_TO_MEMB_SEL,
                                             pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '.' && src[i + 2] == '.') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_ELLIPSIS,
                                                    pos, line_start));
                i += 2;
                pos.column += 2;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_MEMB_SEL,
                                                    pos, line_start));
            }
            break;

        case '*':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_MUL_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_MUL, pos,
                                                    line_start));
            }
            break;

        case '/':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_DIV_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_DIV, pos,
                                                    line_start));
            }
            break;

        case '%':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_MOD_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_MOD, pos,
                                                    line_start));
            }
            break;

        case '+':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_ADD_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '+') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_INC, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_ADD, pos,
                                                    line_start));
            }
            break;

        case '-':
            if (src[i + 1] == '>') {
                if (src[i + 2] == '*')
                    MidGen_dynpush(
                        &toks,
                        create_basic_tok(MIDLEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL,
                                         pos, line_start));
                else
                    MidGen_dynpush(
                        &toks, create_basic_tok(MIDLEXER_TOKENTYPE_PTR_MEMB_SEL,
                                                pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_SUB_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '-') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_DEC, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_SUB, pos,
                                                    line_start));
            }
            break;

        case '<':
            if (src[i + 1] == '<') {
                if (src[i + 1] == '=')
                    MidGen_dynpush(&toks, create_basic_tok(
                                           MIDLEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN,
                                           pos, line_start));
                else
                    MidGen_dynpush(&toks,
                                create_basic_tok(MIDLEXER_TOKENTYPE_LEFT_SHIFT,
                                                 pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_LTEQ,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_LT, pos,
                                                    line_start));
            }
            break;

        case '>':
            if (src[i + 1] == '>') {
                if (src[i + 1] == '=')
                    MidGen_dynpush(
                        &toks,
                        create_basic_tok(MIDLEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN,
                                         pos, line_start));
                else
                    MidGen_dynpush(&toks,
                                create_basic_tok(MIDLEXER_TOKENTYPE_RIGHT_SHIFT,
                                                 pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_GTEQ,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_GT, pos,
                                                    line_start));
            }
            break;

        case '=':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_EQ, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_ASSIGN,
                                                    pos, line_start));
            }
            break;

        case '!':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_NEQ, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_LOGICAL_NOT,
                                             pos, line_start));
            }
            break;

        case '&':
            if (src[i + 1] == '&') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_LOGICAL_AND,
                                             pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_AND_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_BITWISE_AND,
                                             pos, line_start));
            }
            break;

        case '^':
            if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_XOR_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_BITWISE_XOR,
                                             pos, line_start));
            }
            break;

        case '|':
            if (src[i + 1] == '|') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_LOGICAL_OR, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_OR_ASSIGN, pos,
                                             line_start));
                ++i;
                ++pos.column;
            } else {
                MidGen_dynpush(&toks,
                            create_basic_tok(MIDLEXER_TOKENTYPE_BITWISE_OR, pos,
                                             line_start));
            }
            break;

        case ',':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_COMMA, pos,
                                                line_start));
            break;

        case '~':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_BITWISE_NOT,
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
            MidGen_dynpush(&toks,
                        create_numlit_tok(src, i, &i, pos, line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        case '(':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_L_PAREN, pos,
                                                line_start));
            break;
        case ')':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_R_PAREN, pos,
                                                line_start));
            break;
        case '[':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_L_SQBRACKET,
                                                pos, line_start));
            break;
        case ']':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_R_SQBRACKET,
                                                pos, line_start));
            break;
        case '{':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_L_CURLY, pos,
                                                line_start));
            break;
        case '}':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_R_CURLY, pos,
                                                line_start));
            break;

        case ';':
            MidGen_dynpush(&toks, create_basic_tok(MIDLEXER_TOKENTYPE_SEMICOLON,
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
                MidGen_dynpush(&toks, tok);
                // if the symbol is an actual identifier then it needs to be
                // added to the symbol table, otherwise the identifier can be
                // discarded
                if (!in_tbl && tok.type == MIDLEXER_TOKENTYPE_IDENTIFIER)
                    MidGen_dynpush(&symbtbl, id);
                else if (!in_tbl)
                    free(id);
                pos.column += i - old_i;
                break;
            }
        }

        case '\'': {
        parse_char_lit:
            auto old_i = i;
            MidGen_dynpush(
                &toks, create_charlit_tok(src, i, &i, pos, line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        case '"': {
        parse_str_lit:
            auto old_i = i;
            MidGen_dynpush(&toks, create_strlit_tok(src, &str_lits, i, &i, pos,
                                                 line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        default: {
            // column isn't updated cuz it's still one character
            char *c = MidUTF8_char_to_str(MidUTF8_read_char(src, i, &i));
            --i;
            struct MidDiag_Diag err = {
                .type = MIDDIAG_TYPE_ERROR,
                .err = MIDDIAG_ERR_UNKNOWN_SYMBOL,
                .msg = MidPrint_fmt_to_str("unknown symbol '%s'", c),
                .pos = pos,
                .line = line_start};
            MidGen_dynpush(&diags, err);
            free(c);
            break;
        }
        }
    }
    MidGen_dynpush(&toks,
                create_basic_tok(MIDLEXER_TOKENTYPE_END, pos, line_start));

    struct MidLexer_Tokenize ret;
    ret.toks = toks;
    ret.symtbl = symbtbl;
    ret.str_lits = str_lits;
    ret.diags = diags;
    return ret;
}

struct MidLexer_Tokenize MidLexer_tokenize(const char *src, const char *file)
{
    auto lex = read_tokens(src, file);

    return lex;
}

void MidLexer_Tokenize_deinit(struct MidLexer_Tokenize *self)
{
    MidGen_dyndeinit(&self->toks);
    MidGen_dyndeinit(&self->symtbl, MidSymbol_deinit_symbol);
    MidGen_dyndeinit(&self->str_lits, MidLit_String_deinit);
    MidGen_dyndeinit(&self->diags, MidDiag_deinit);
}
