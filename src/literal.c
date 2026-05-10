#include "literal.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "parser/expr_type.h"
#include "types.h"
#include "utf8.h"
#include <stdio.h>
#include <wchar.h>

void Literal_String_deinit(struct Literal_String *self)
{
    switch (self->type) {
    case LITERAL_STRINGTYPE_CHAR:
        free(self->c);
        break;

    case LITERAL_STRINGTYPE_WCHAR:
        free(self->wc);
        break;

    case LITERAL_STRINGTYPE_CHAR16:
        free(self->c16);
        break;

    case LITERAL_STRINGTYPE_CHAR32:
        free(self->c32);
        break;
    }
}

static isize_t c_str_len(const TypesCharType *str)
{
    isize_t i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static isize_t wc_str_len(const TypesWCharType *str)
{
    isize_t i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static isize_t c16_str_len(const u16 *str)
{
    isize_t i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

static isize_t c32_str_len(const u32 *str)
{
    isize_t i;
    for (i = 0; str[i] != '\0'; ++i)
        ;
    return i;
}

isize_t Literal_strlit_len(const struct Literal_String *strlit)
{
    switch (strlit->type) {
    case LITERAL_STRINGTYPE_CHAR:
        return c_str_len(strlit->c);

    case LITERAL_STRINGTYPE_WCHAR:
        return wc_str_len(strlit->wc);

    case LITERAL_STRINGTYPE_CHAR16:
        return c16_str_len(strlit->c16);

    case LITERAL_STRINGTYPE_CHAR32:
        return c32_str_len(strlit->c32);
    }
}

void Literal_print(union Literal_Value val, enum Parser_ExprType type)
{
    switch (type) {
    case PARSER_EXPRTYPE_CHAR_LIT:
        printf("'%c'", (char)val.sint);
        break;

    case PARSER_EXPRTYPE_WCHAR_LIT:
        printf("'%c'", (wchar_t)val.sint);
        break;

    case PARSER_EXPRTYPE_CHAR16_LIT:
    case PARSER_EXPRTYPE_CHAR32_LIT:
        putchar('\'');
        UTF8_print_char(val.uint);
        putchar('\'');
        break;

    case PARSER_EXPRTYPE_STRING_LIT:
        printf("\"%s\"", val.str.c);
        break;

    case PARSER_EXPRTYPE_WSTRING_LIT:
        putchar('"');
        static_assert(Types_wchar_size == 2 || Types_wchar_size == 4);
        if (Types_wchar_size == 2)
            UTF8_print_str16((void *)val.str.wc);
        else
            UTF8_print_str32((void *)val.str.wc);
        putchar('"');
        break;

    case PARSER_EXPRTYPE_STRING16_LIT:
        putchar('"');
        UTF8_print_str16(val.str.c16);
        putchar('"');

    case PARSER_EXPRTYPE_STRING32_LIT:
        putchar('"');
        UTF8_print_str32(val.str.c32);
        putchar('"');
        break;

    case PARSER_EXPRTYPE_INT_LIT:
    case PARSER_EXPRTYPE_LONG_LIT:
    case PARSER_EXPRTYPE_LONGLONG_LIT:
        printf("%" PRIi64, val.sint);
        break;

    case PARSER_EXPRTYPE_UINT_LIT:
    case PARSER_EXPRTYPE_ULONG_LIT:
    case PARSER_EXPRTYPE_ULONGLONG_LIT:
        printf("%" PRIu64, val.uint);
        break;

    case PARSER_EXPRTYPE_FLOAT_LIT:
    case PARSER_EXPRTYPE_DOUBLE_LIT:
    case PARSER_EXPRTYPE_LONGDOUBLE_LIT:
        printf("%Lf", val.flt);
        break;

    case PARSER_EXPRTYPE_BOOL_LIT:
        printf("%s", val.sint ? "true" : "false");
        break;

    case PARSER_EXPRTYPE_PTR_LIT:
        printf("%p", val.ptr);
        break;

    default:
        CRASH("expr is not a literal");
    }
}

void Literal_print_toktype(union Literal_Value val, enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_CHAR_LIT:
        Literal_print(val, PARSER_EXPRTYPE_CHAR_LIT);
        break;

    case LEXER_TOKENTYPE_WCHAR_LIT:
        Literal_print(val, PARSER_EXPRTYPE_WCHAR_LIT);
        break;

    case LEXER_TOKENTYPE_CHAR16_LIT:
        Literal_print(val, PARSER_EXPRTYPE_CHAR16_LIT);
        break;

    case LEXER_TOKENTYPE_CHAR32_LIT:
        Literal_print(val, PARSER_EXPRTYPE_CHAR32_LIT);
        break;

    case LEXER_TOKENTYPE_STRING_LIT:
        Literal_print(val, PARSER_EXPRTYPE_STRING_LIT);
        break;

    case LEXER_TOKENTYPE_WSTRING_LIT:
        Literal_print(val, PARSER_EXPRTYPE_WSTRING_LIT);
        break;

    case LEXER_TOKENTYPE_STRING16_LIT:
        Literal_print(val, PARSER_EXPRTYPE_STRING16_LIT);
        break;

    case LEXER_TOKENTYPE_STRING32_LIT:
        Literal_print(val, PARSER_EXPRTYPE_STRING32_LIT);
        break;

    case LEXER_TOKENTYPE_INT_LIT:
        Literal_print(val, PARSER_EXPRTYPE_INT_LIT);
        break;

    case LEXER_TOKENTYPE_UINT_LIT:
        Literal_print(val, PARSER_EXPRTYPE_UINT_LIT);
        break;

    case LEXER_TOKENTYPE_LONG_LIT:
        Literal_print(val, PARSER_EXPRTYPE_LONG_LIT);
        break;

    case LEXER_TOKENTYPE_ULONG_LIT:
        Literal_print(val, PARSER_EXPRTYPE_ULONG_LIT);
        break;

    case LEXER_TOKENTYPE_LONGLONG_LIT:
        Literal_print(val, PARSER_EXPRTYPE_LONGLONG_LIT);
        break;

    case LEXER_TOKENTYPE_ULONGLONG_LIT:
        Literal_print(val, PARSER_EXPRTYPE_ULONGLONG_LIT);
        break;

    case LEXER_TOKENTYPE_FLOAT_LIT:
        Literal_print(val, PARSER_EXPRTYPE_FLOAT_LIT);
        break;

    case LEXER_TOKENTYPE_DOUBLE_LIT:
        Literal_print(val, PARSER_EXPRTYPE_DOUBLE_LIT);
        break;

    case LEXER_TOKENTYPE_LONGDOUBLE_LIT:
        Literal_print(val, PARSER_EXPRTYPE_LONGDOUBLE_LIT);
        break;

    case LEXER_TOKENTYPE_BOOL_LIT:
        Literal_print(val, PARSER_EXPRTYPE_BOOL_LIT);
        break;

    case LEXER_TOKENTYPE_PTR_LIT:
        Literal_print(val, PARSER_EXPRTYPE_PTR_LIT);
        break;

    default:
        CRASH("token is not literal");
    }
}
