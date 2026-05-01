#include "tokenize.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "position.h"
#include "print.h"
#include "symbol.h"
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
    char *str = malloc((len + 1) * sizeof(*str));
    str[len] = '\0';

    for (isize_t i = 0; i < len; ++i)
        str[i] = src[i + start];

    return str;
}

static struct Lexer_Token create_identifier_tok(char *id, struct Position pos,
                                                const char *line)
{
    if (!strcmp(id, "char"))
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
                                    .val.bl = true};
    else if (!strcmp(id, "false"))
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_BOOL_LIT,
                                    .val.bl = false};
    else if (!strcmp(id, "nullptr"))
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_PTR_LIT,
                                    .val.ptr = NULL};

    else
        return (struct Lexer_Token){.pos = pos,
                                    .line = line,
                                    .type = LEXER_TOKENTYPE_IDENTIFIER,
                                    .ident = id};
}

static struct Lexer_Tokenize read_tokens(const char *src, const char *file)
{
    struct Lexer_TokenVec toks = gen_dyninit();
    struct SymbolTable symbtbl = gen_dyninit();
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

        CASE_ISALPHA:
        case '_': {
            auto old_i = i;
            char *id = read_identifier(src, i, &i);
            --i;
            auto tok = create_identifier_tok(id, pos, line_start);
            gen_dynpush(&toks, tok);
            // if the symbol is an actual identifier then it needs to be added
            // to the symbol table, otherwise the identifier can be discarded
            if (tok.type == LEXER_TOKENTYPE_IDENTIFIER)
                gen_dynpush(&symbtbl, id);
            else
                free(id);
            pos.column += i - old_i;
            break;
        }

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
    gen_dynpush(&toks, create_basic_tok(LEXER_TOKENTYPE_END, pos, line_start));

    struct Lexer_Tokenize ret;
    ret.toks = toks;
    ret.symtbl = symbtbl;
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
    gen_dyndeinit(&self->diags, Diag_deinit);
}
