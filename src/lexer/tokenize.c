#include "tokenize.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "position.h"
#include "print.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            return NUMLIT_LONGLONG;
            if (suffix_end)
                *suffix_end = end + 2;
        }
        if (suffix_end)
            *suffix_end = end + 1;
        return is_decimal ? NUMLIT_LONGDOUBLE : NUMLIT_LONG;
    } else if (c0 == 'f') {
        if (suffix_end)
            *suffix_end = end + 1;
        return NUMLIT_FLOAT;
    } else if (c0 == 'd') {
        if (suffix_end)
            *suffix_end = end + 1;
        return NUMLIT_DOUBLE;
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
static struct NumLit read_numlit(const char *src, isize_t start, isize_t *end)
{
    isize_t digits_end = find_numlit_digits_end(src, start);
    isize_t lit_end;
    auto is_decimal = numlit_is_decimal(src, start, digits_end);
    auto type = numlit_type(src, digits_end, is_decimal, &lit_end);

    if (end)
        *end = lit_end;

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
        ret.val.flt = strtof(&src[start], NULL);
        break;

    case NUMLIT_DOUBLE:
        ret.val.dbl = strtod(&src[start], NULL);
        break;

    case NUMLIT_LONGDOUBLE:
        ret.val.l_dbl = strtold(&src[start], NULL);
        break;
    }

    Dynstr_deinit(&str);

    return ret;
}

// end - out variable and can be NULL
static struct Lexer_Token create_numlit_tok(const char *src, isize_t start,
                                            isize_t *end, struct Position pos,
                                            const char *line)
{
    auto info = read_numlit(src, start, end);

    struct Lexer_Token ret;
    ret.pos = pos;
    ret.line = line;
    ret.val = info.val;

    switch (info.type) {
    case NUMLIT_INT:
        ret.type = LEXER_TOKENTYPE_INT_LIT;
        break;

    case NUMLIT_UINT:
        ret.type = LEXER_TOKENTYPE_UINT_LIT;
        break;

    case NUMLIT_LONG:
        ret.type = LEXER_TOKENTYPE_LONG_LIT;
        break;

    case NUMLIT_ULONG:
        ret.type = LEXER_TOKENTYPE_ULONG_LIT;
        break;

    case NUMLIT_LONGLONG:
        ret.type = LEXER_TOKENTYPE_LONGLONG_LIT;
        break;

    case NUMLIT_ULONGLONG:
        ret.type = LEXER_TOKENTYPE_ULONGLONG_LIT;
        break;

    case NUMLIT_FLOAT:
        ret.type = LEXER_TOKENTYPE_FLOAT_LIT;
        break;

    case NUMLIT_DOUBLE:
        ret.type = LEXER_TOKENTYPE_DOUBLE_LIT;
        break;

    case NUMLIT_LONGDOUBLE:
        ret.type = LEXER_TOKENTYPE_LONGDOUBLE_LIT;
        break;
    }

    return ret;
}

struct Lexer_Tokenize Lexer_tokenize(const char *src, const char *file)
{
    struct Lexer_TokenVec toks = gen_dyninit();
    struct DiagVec diags = gen_dyninit();
    struct Position pos = {.file = file, .line = 1, .column = 1};

    const char *line_start = src;

    for (isize_t i = 0; src[i] != '\0'; ++i) {
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

        case '+':
            gen_dynpush(&toks,
                        create_basic_tok(LEXER_TOKENTYPE_ADD, pos, line_start));
            break;
        case '-':
            gen_dynpush(&toks,
                        create_basic_tok(LEXER_TOKENTYPE_SUB, pos, line_start));
            break;
        case '*':
            gen_dynpush(&toks,
                        create_basic_tok(LEXER_TOKENTYPE_MUL, pos, line_start));
            break;
        case '/':
            gen_dynpush(&toks,
                        create_basic_tok(LEXER_TOKENTYPE_DIV, pos, line_start));
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
        case '.':
            if (isdigit(src[i + 1])) { // literals like .5
                auto old_i = i;
                gen_dynpush(&toks,
                            create_numlit_tok(src, i, &i, pos, line_start));
                --i;
                pos.column += i - old_i;
            }
            break;

        case '(':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_L_PAREN, pos,
                                                line_start));
            break;
        case ')':
            gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_R_PAREN, pos,
                                                line_start));
            break;

        default:
            struct Diag err = {
                .is_err = true,
                .err = ERRORTYPE_UNKNOWN_SYMBOL,
                .msg = Print_fmt_to_str("unknown symbol '%c'", src[i]),
                .pos = pos,
                .line = line_start};
            gen_dynpush(&diags, err);
            break;
        }

        ++pos.column;
    }

    struct Lexer_Tokenize ret;
    ret.toks = toks;
    ret.diags = diags;
    return ret;
}

void Lexer_Tokenize_deinit(struct Lexer_Tokenize *self)
{
    gen_dyndeinit(&self->toks);
    gen_dyndeinit(&self->diags, Diag_deinit);
}
