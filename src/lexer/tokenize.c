#include "tokenize.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
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

static struct Lexer_Token create_basic_tok(enum Lexer_TokenType type,
                                           struct Position pos,
                                           const char *line)
{
    return (struct Lexer_Token){.type = type, .pos = pos, .line = line};
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

static isize_t find_numlit_digits_end(const char *src, isize_t start)
{
    bool is_decimal = src[start] == '.';

    isize_t end;
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
static enum NumLitType numlit_type(const char *src, isize_t end,
                                   bool is_decimal, isize_t *suffix_end)
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

static bool numlit_is_decimal(const char *src, isize_t start, isize_t end)
{
    for (isize_t i = start; i < end; ++i) {
        if (src[i] == '.')
            return true;
    }

    return false;
}

struct NumLit {
    union Literal_Value val;
    enum NumLitType type;
};

// end - out variable and can be NULL
static struct NumLit read_numlit(const char *src, isize_t start,
                                 isize_t *out_end)
{
    isize_t digits_end = find_numlit_digits_end(src, start);
    isize_t lit_end;
    auto is_decimal = numlit_is_decimal(src, start, digits_end);
    auto type = numlit_type(src, digits_end, is_decimal, &lit_end);

    if (out_end)
        *out_end = lit_end;

    struct Dynstr str = Dynstr();
    for (isize_t i = start; i < lit_end; ++i)
        Dynstr_append_char(&str, src[i]);

    struct NumLit ret;
    ret.type = type;

    switch (type) {
    case NUMLIT_INT:
    case NUMLIT_LONG:
    case NUMLIT_LONGLONG:
        ret.val.sint = strtoll(&src[start], NULL, 0);
        break;

    case NUMLIT_UINT:
    case NUMLIT_ULONG:
    case NUMLIT_ULONGLONG:
        ret.val.uint = strtoull(&src[start], NULL, 0);
        break;

    case NUMLIT_FLOAT:
    case NUMLIT_DOUBLE:
    case NUMLIT_LONGDOUBLE:
        ret.val.flt = strtold(&src[start], NULL);
        break;
    }

    Dynstr_deinit(&str);

    return ret;
}

static enum Lexer_TokenType numlit_type_to_tok_type(enum NumLitType type)
{
    switch (type) {
    case NUMLIT_INT:
        return LEXER_TOKENTYPE_INT_LIT;

    case NUMLIT_UINT:
        return LEXER_TOKENTYPE_UINT_LIT;

    case NUMLIT_LONG:
        return LEXER_TOKENTYPE_LONG_LIT;

    case NUMLIT_ULONG:
        return LEXER_TOKENTYPE_ULONG_LIT;

    case NUMLIT_LONGLONG:
        return LEXER_TOKENTYPE_LONGLONG_LIT;

    case NUMLIT_ULONGLONG:
        return LEXER_TOKENTYPE_ULONGLONG_LIT;

    case NUMLIT_FLOAT:
        return LEXER_TOKENTYPE_FLOAT_LIT;

    case NUMLIT_DOUBLE:
        return LEXER_TOKENTYPE_DOUBLE_LIT;

    case NUMLIT_LONGDOUBLE:
        return LEXER_TOKENTYPE_LONGDOUBLE_LIT;
    }
}

// end - out variable and can be NULL
static struct Lexer_Token create_numlit_tok(const char *src, isize_t start,
                                            isize_t *out_end,
                                            struct Position pos,
                                            const char *line)
{
    auto info = read_numlit(src, start, out_end);

    struct Lexer_Token ret;
    ret.pos = pos;
    ret.line = line;
    ret.val = info.val;
    ret.type = numlit_type_to_tok_type(info.type);
    return ret;
}

enum Literal_StringType charlit_type(const char *src, isize_t start,
                                     isize_t *prefix_end, struct Position pos,
                                     const char *line, struct DiagVec *diags)
{
    if (prefix_end)
        *prefix_end = start + 1;

    switch (src[start]) {
    case '\'':
    case '"':
        if (prefix_end)
            *prefix_end = start;
        return LITERAL_STRINGTYPE_CHAR;

    case 'u':
        if (prefix_end)
            *prefix_end = start + 1;
        return LITERAL_STRINGTYPE_CHAR16;

    case 'U':
        if (prefix_end)
            *prefix_end = start + 1;
        return LITERAL_STRINGTYPE_CHAR32;

    case 'L':
        if (prefix_end)
            *prefix_end = start + 1;
        return LITERAL_STRINGTYPE_WCHAR;

    default:
        gen_dynpush(diags,
                    ((struct Diag){
                        .pos = pos,
                        .line = line,
                        .msg = Print_fmt_to_str(
                            "unknown char literal prefix '%c'", src[start]),
                        .err = ERRORTYPE_BAD_LITERAL,
                        .is_err = true,
                    }));
        return LITERAL_STRINGTYPE_CHAR;
    }
}

static struct Diag expected_tok_err(const char *name, struct Position pos,
                                    const char *line, enum ErrorType type)
{
    return (struct Diag){
        .pos = pos,
        .line = line,
        .msg = Print_fmt_to_str("expected %s", name),
        .err = type,
        .is_err = true,
    };
}

static enum Lexer_TokenType
charlit_type_to_tok_type(enum Literal_StringType type)
{
    switch (type) {
    case LITERAL_STRINGTYPE_CHAR:
        return LEXER_TOKENTYPE_CHAR_LIT;

    case LITERAL_STRINGTYPE_WCHAR:
        return LEXER_TOKENTYPE_WCHAR_LIT;

    case LITERAL_STRINGTYPE_CHAR16:
        return LEXER_TOKENTYPE_CHAR16_LIT;

    case LITERAL_STRINGTYPE_CHAR32:
        return LEXER_TOKENTYPE_CHAR32_LIT;
    }
}

static enum Lexer_TokenType
charlit_type_to_str_tok_type(enum Literal_StringType type)
{
    switch (type) {
    case LITERAL_STRINGTYPE_CHAR:
        return LEXER_TOKENTYPE_STRING_LIT;

    case LITERAL_STRINGTYPE_WCHAR:
        return LEXER_TOKENTYPE_WSTRING_LIT;

    case LITERAL_STRINGTYPE_CHAR16:
        return LEXER_TOKENTYPE_STRING16_LIT;

    case LITERAL_STRINGTYPE_CHAR32:
        return LEXER_TOKENTYPE_STRING32_LIT;
    }
}

bool verify_charlit_value(u32 val, enum Literal_StringType type,
                          struct Position pos, const char *line,
                          struct DiagVec *diags)
{
    bool too_big = false;

    switch (type) {
    case LITERAL_STRINGTYPE_CHAR:
        too_big = val > Types_char_umax;
        break;

    case LITERAL_STRINGTYPE_WCHAR:
        too_big = val > Types_wchar_umax;
        break;

    case LITERAL_STRINGTYPE_CHAR16:
        too_big = val > UINT16_MAX;
        break;

    case LITERAL_STRINGTYPE_CHAR32:
        // val is exactly 32 bits
        break;
    }

    if (too_big)
        gen_dynpush(diags,
                    ((struct Diag){
                        .pos = pos,
                        .line = line,
                        .msg = Print_fmt_to_str(
                            "character to big to fit in character literal"),
                        .err = ERRORTYPE_BAD_LITERAL,
                        .is_err = true,
                    }));

    return !too_big;
}

static struct Lexer_Token
create_charlit_tok(const char *src, isize_t start, isize_t *out_end,
                   struct Position pos, const char *line, struct DiagVec *diags)
{
    isize_t lquote;
    auto type = charlit_type(src, start, &lquote, pos, line, diags);
    // control flow shouldn't get here otherwise but better safe than sorry
    assert(src[lquote] == '\'');

    struct Lexer_Token ret = {};
    ret.pos = pos;
    ret.line = line;
    ret.type = charlit_type_to_tok_type(type);
    isize_t rquote;
    ret.val.uint = UTF8_read_char(src, lquote + 1, &rquote);

    if (!verify_charlit_value(ret.val.uint, type, pos, line, diags))
        ret.val.uint = '\0';

    if (src[rquote] != '\'') {
        gen_dynpush(diags,
                    expected_tok_err("'", pos, line, ERRORTYPE_MISSING_QUOTE));
        if (out_end)
            *out_end = rquote;
    } else if (out_end) {
        *out_end = rquote + 1;
    }

    return ret;
}

void realloc_strlit(struct Literal_String *str, isize_t cap)
{
    switch (str->type) {
    case LITERAL_STRINGTYPE_CHAR:
        str->c = mid_realloc(str->c, cap * sizeof(*str->c));
        break;

    case LITERAL_STRINGTYPE_WCHAR:
        str->wc = mid_realloc(str->wc, cap * sizeof(*str->wc));
        break;

    case LITERAL_STRINGTYPE_CHAR16:
        str->c16 = mid_realloc(str->c16, cap * sizeof(*str->c16));
        break;

    case LITERAL_STRINGTYPE_CHAR32:
        str->c32 = mid_realloc(str->c32, cap * sizeof(*str->c32));
        break;
    }
}

static void strlit_add(struct Literal_String *str, isize_t idx, u32 c)
{
    switch (str->type) {
    case LITERAL_STRINGTYPE_CHAR:
        str->c[idx] = c;
        break;

    case LITERAL_STRINGTYPE_WCHAR:
        str->wc[idx] = c;
        break;

    case LITERAL_STRINGTYPE_CHAR16:
        str->c16[idx] = c;
        break;

    case LITERAL_STRINGTYPE_CHAR32:
        str->c32[idx] = c;
        break;
    }
}

struct Literal_String read_strlit(const char *src, isize_t lquote,
                                  isize_t *out_end,
                                  enum Literal_StringType type,
                                  struct Position pos, const char *line,
                                  struct DiagVec *diags)
{
    isize_t len = 0;
    isize_t cap = 128;
    struct Literal_String str = {.type = type};
    realloc_strlit(&str, cap);

    isize_t i;
    for (i = lquote + 1; src[i] != '"' && src[i] != '\n';) {
        u32 c = UTF8_read_char(src, i, &i);
        verify_charlit_value(c, type, pos, line, diags);

        strlit_add(&str, len++, c);
        if (len == cap)
            realloc_strlit(&str, cap += 128);
    }

    strlit_add(&str, len, '\0');

    if (src[i] != '"')
        gen_dynpush(diags,
                    expected_tok_err("\"", pos, line, ERRORTYPE_BAD_LITERAL));

    if (out_end)
        *out_end = i + (src[i] == '"');
    return str;
}

static struct Lexer_Token
create_strlit_tok(const char *src, struct Literal_StringVec *str_lits,
                  isize_t start, isize_t *out_end, struct Position pos,
                  const char *line, struct DiagVec *diags)
{
    isize_t lquote;
    auto type = charlit_type(src, start, &lquote, pos, line, diags);
    assert(src[lquote] == '"');

    struct Lexer_Token ret = {};
    ret.pos = pos;
    ret.line = line;
    ret.type = charlit_type_to_str_tok_type(type);

    auto lit = read_strlit(src, lquote, out_end, type, pos, line, diags);
    gen_dynpush(str_lits, lit);
    ret.val.str = lit;

    return ret;
}

static bool is_identifier_char(char c)
{
    return isalnum(c) || c == '_';
}

static isize_t identifier_end(const char *src, isize_t start)
{
    isize_t end;
    for (end = start; is_identifier_char(src[end]); ++end)
        ;
    return end;
}

// end - an out variable and can be NULL
static char *read_identifier(const char *src, isize_t start, isize_t *end)
{
    isize_t id_end = identifier_end(src, start);
    if (end)
        *end = id_end;

    isize_t len = id_end - start;
    char *str = mid_malloc((len + 1) * sizeof(*str));
    str[len] = '\0';

    for (isize_t i = 0; i < len; ++i)
        str[i] = src[i + start];

    return str;
}

static struct Lexer_Token create_identifier_tok(char *id, struct Position pos,
                                                const char *line)
{
    if (!strcmp(id, "void"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_VOID};
    else if (!strcmp(id, "char"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CHAR};
    else if (!strcmp(id, "wchar_t"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_WCHAR};
    else if (!strcmp(id, "char16_t"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CHAR16};
    else if (!strcmp(id, "char32_t"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CHAR32};
    else if (!strcmp(id, "bool"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_BOOL};
    else if (!strcmp(id, "int"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_INT};
    else if (!strcmp(id, "float"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_FLOAT};
    else if (!strcmp(id, "double"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_DOUBLE};
    else if (!strcmp(id, "class"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CLASS};
    else if (!strcmp(id, "struct"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_STRUCT};
    else if (!strcmp(id, "union"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_UNION};
    else if (!strcmp(id, "enum"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_ENUM};
    else if (!strcmp(id, "auto"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_AUTO};

    else if (!strcmp(id, "short"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_SHORT};
    else if (!strcmp(id, "long"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_LONG};
    else if (!strcmp(id, "signed"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_SIGNED};
    else if (!strcmp(id, "unsigned"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_UNSIGNED};
    else if (!strcmp(id, "static"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_STATIC};
    else if (!strcmp(id, "constexpr"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CONSTEXPR};
    else if (!strcmp(id, "typedef"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_TYPEDEF};

    else if (!strcmp(id, "const"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CONST};

    else if (!strcmp(id, "typeid"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_TYPEID};

    else if (!strcmp(id, "const_cast"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_CONSTCAST};
    else if (!strcmp(id, "dynamic_cast"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_DYNAMICCAST};
    else if (!strcmp(id, "reinterpret_cast"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_REINTERPRETCAST};
    else if (!strcmp(id, "static_cast"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_STATICCAST};

    else if (!strcmp(id, "new"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_NEW};
    else if (!strcmp(id, "delete"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_DELETE};

    else if (!strcmp(id, "throw"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_THROW};

    else if (!strcmp(id, "true"))
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_BOOL_LIT,
                                    .val.sint = true};
    else if (!strcmp(id, "false"))
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_BOOL_LIT,
                                    .val.sint = false};
    else if (!strcmp(id, "nullptr"))
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_PTR_LIT,
                                    .val.ptr = NULL};

    else if (!strcmp(id, "public"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_PUBLIC};
    else if (!strcmp(id, "private"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_PRIVATE};
    else if (!strcmp(id, "protected"))
        return (struct Lexer_Token){
            .pos = pos, .line = line, .type = LEXER_TOKENTYPE_PROTECTED};

    else
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_IDENTIFIER,
                                    .ident = id};
}

static isize_t skip_to_line_end(const char *src, isize_t start,
                                struct Position *pos)
{
    isize_t i = start;
    while (src[++i] != '\n')
        ++pos->column;
    return i - 1;
}

// c style comment: /* ... */
//                  ^
//                start
//                          ^
//                        return
static isize_t skip_c_comment(const char *src, isize_t start,
                              struct Position *pos)
{
    isize_t i = start + 2;

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
static char *symb_in_tbl(struct SymbolTable *tbl, const char *symb)
{
    for (isize_t i = 0; i < tbl->len; ++i) {
        if (!strcmp(tbl->arr[i], symb))
            return tbl->arr[i];
    }

    return NULL;
}

static struct Lexer_Tokenize read_tokens(const char *src, const char *file)
{
    struct Lexer_TokenVec toks = {};
    struct SymbolTable symbtbl = {};
    struct Literal_StringVec str_lits = {};
    struct DiagVec diags = {};
    struct Position pos = {.file = file, .line = 1, .column = 1};

    const char *line_start = src;

    for (isize_t i = 0; src[i] != '\0'; ++i, ++pos.column) {
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
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_SCOPE_RES,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_COLON, pos,
                                                    line_start));
            }
            break;

        case '.':
            if (isdigit(src[i + 1])) { // literals like .5
                auto old_i = i;
                gen_dynpush(&toks,
                            create_numlit_tok(src, i, &i, pos, line_start));
                --i;
                pos.column += i - old_i;
            } else if (src[i + 1] == '*') {
                gen_dynpush(&toks,
                            create_basic_tok(LEXER_TOKENTYPE_PTR_TO_MEMB_SEL,
                                             pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '.' && src[i + 2] == '.') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_ELLIPSIS,
                                                    pos, line_start));
                i += 2;
                pos.column += 2;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_MEMB_SEL,
                                                    pos, line_start));
            }
            break;

        case '*':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_MUL_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_MUL, pos,
                                                    line_start));
            }
            break;

        case '/':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_DIV_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_DIV, pos,
                                                    line_start));
            }
            break;

        case '%':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_MOD_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_MOD, pos,
                                                    line_start));
            }
            break;

        case '+':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_ADD_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '+') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_INC, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_ADD, pos,
                                                    line_start));
            }
            break;

        case '-':
            if (src[i + 1] == '>') {
                if (src[i + 2] == '*')
                    gen_dynpush(&toks, create_basic_tok(
                                           LEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL,
                                           pos, line_start));
                else
                    gen_dynpush(&toks,
                                create_basic_tok(LEXER_TOKENTYPE_PTR_MEMB_SEL,
                                                 pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_SUB_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '-') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_DEC, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_SUB, pos,
                                                    line_start));
            }
            break;

        case '<':
            if (src[i + 1] == '<') {
                if (src[i + 1] == '=')
                    gen_dynpush(&toks, create_basic_tok(
                                           LEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN,
                                           pos, line_start));
                else
                    gen_dynpush(&toks,
                                create_basic_tok(LEXER_TOKENTYPE_LEFT_SHIFT,
                                                 pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_LTEQ, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_LT, pos,
                                                    line_start));
            }
            break;

        case '>':
            if (src[i + 1] == '>') {
                if (src[i + 1] == '=')
                    gen_dynpush(&toks, create_basic_tok(
                                           LEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN,
                                           pos, line_start));
                else
                    gen_dynpush(&toks,
                                create_basic_tok(LEXER_TOKENTYPE_RIGHT_SHIFT,
                                                 pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_GTEQ, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_GT, pos,
                                                    line_start));
            }
            break;

        case '=':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_EQ, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_ASSIGN, pos,
                                                    line_start));
            }
            break;

        case '!':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_NEQ, pos,
                                                    line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_LOGICAL_NOT,
                                                    pos, line_start));
            }
            break;

        case '&':
            if (src[i + 1] == '&') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_LOGICAL_AND,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_AND_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_BITWISE_AND,
                                                    pos, line_start));
            }
            break;

        case '^':
            if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_XOR_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_BITWISE_XOR,
                                                    pos, line_start));
            }
            break;

        case '|':
            if (src[i + 1] == '|') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_LOGICAL_OR,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else if (src[i + 1] == '=') {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_OR_ASSIGN,
                                                    pos, line_start));
                ++i;
                ++pos.column;
            } else {
                gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_BITWISE_OR,
                                                    pos, line_start));
            }
            break;

        case ',':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_COMMA, pos,
                                                line_start));
            break;

        case '~':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_BITWISE_NOT,
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
            gen_dynpush(&toks, create_numlit_tok(src, i, &i, pos, line_start));
            --i;
            pos.column += i - old_i;
            break;
        }

        case '(':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_L_PAREN, pos,
                                                line_start));
            break;
        case ')':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_R_PAREN, pos,
                                                line_start));
            break;
        case '[':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_L_SQBRACKET,
                                                pos, line_start));
            break;
        case ']':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_R_SQBRACKET,
                                                pos, line_start));
            break;
        case '{':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_L_CURLY, pos,
                                                line_start));
            break;
        case '}':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_R_CURLY, pos,
                                                line_start));
            break;

        case ';':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_SEMICOLON, pos,
                                                line_start));
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
                gen_dynpush(&toks, tok);
                // if the symbol is an actual identifier then it needs to be
                // added to the symbol table, otherwise the identifier can be
                // discarded
                if (!in_tbl && tok.type == LEXER_TOKENTYPE_IDENTIFIER)
                    gen_dynpush(&symbtbl, id);
                else if (!in_tbl)
                    free(id);
                pos.column += i - old_i;
                break;
            }
        }

        case '\'': {
        parse_char_lit:
            auto old_i = i;
            gen_dynpush(
                &toks, create_charlit_tok(src, i, &i, pos, line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        case '"': {
        parse_str_lit:
            auto old_i = i;
            gen_dynpush(&toks, create_strlit_tok(src, &str_lits, i, &i, pos,
                                                 line_start, &diags));
            --i;
            pos.column += i - old_i;
            break;
        }

        default: {
            // column isn't updated cuz it's still one character
            char *c = UTF8_char_to_str(UTF8_read_char(src, i, &i));
            --i;
            struct Diag err = {.is_err = true,
                               .err = ERRORTYPE_UNKNOWN_SYMBOL,
                               .msg =
                                   Print_fmt_to_str("unknown symbol '%s'", c),
                               .pos = pos,
                               .line = line_start};
            gen_dynpush(&diags, err);
            free(c);
            break;
        }
        }
    }
    gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_END, pos, line_start));

    struct Lexer_Tokenize ret;
    ret.toks = toks;
    ret.symtbl = symbtbl;
    ret.str_lits = str_lits;
    ret.diags = diags;
    return ret;
}

struct Lexer_Tokenize Lexer_tokenize(const char *src, const char *file)
{
    auto lex = read_tokens(src, file);

    return lex;
}

void Lexer_Tokenize_deinit(struct Lexer_Tokenize *self)
{
    gen_dyndeinit(&self->toks);
    gen_dyndeinit(&self->symtbl, Symbol_deinit_symbol);
    gen_dyndeinit(&self->str_lits, Literal_String_deinit);
    gen_dyndeinit(&self->diags, Diag_deinit);
}
