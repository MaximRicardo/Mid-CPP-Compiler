#include "type.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include <assert.h>
#include <string.h>

enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_CHAR_SPEC:
        return PARSER_TYPESPEC_CHAR;

    case LEXER_TOKENTYPE_SCHAR_SPEC:
        return PARSER_TYPESPEC_SCHAR;

    case LEXER_TOKENTYPE_UCHAR_SPEC:
        return PARSER_TYPESPEC_UCHAR;

    case LEXER_TOKENTYPE_SHORT_SPEC:
        return PARSER_TYPESPEC_SHORT;

    case LEXER_TOKENTYPE_USHORT_SPEC:
        return PARSER_TYPESPEC_USHORT;

    case LEXER_TOKENTYPE_INT_SPEC:
        return PARSER_TYPESPEC_INT;

    case LEXER_TOKENTYPE_UINT_SPEC:
        return PARSER_TYPESPEC_UINT;

    case LEXER_TOKENTYPE_LONG_SPEC:
        return PARSER_TYPESPEC_LONG;

    case LEXER_TOKENTYPE_ULONG_SPEC:
        return PARSER_TYPESPEC_ULONG;

    case LEXER_TOKENTYPE_LONGLONG_SPEC:
        return PARSER_TYPESPEC_LONGLONG;

    case LEXER_TOKENTYPE_ULONGLONG_SPEC:
        return PARSER_TYPESPEC_ULONGLONG;

    case LEXER_TOKENTYPE_FLOAT_SPEC:
        return PARSER_TYPESPEC_FLOAT;

    case LEXER_TOKENTYPE_DOUBLE_SPEC:
        return PARSER_TYPESPEC_DOUBLE;

    case LEXER_TOKENTYPE_LONGDOUBLE_SPEC:
        return PARSER_TYPESPEC_LONGDOUBLE;

    default:
        assert(false);
    }
}

enum Parser_TypeMod Parser_toktype_to_typemod(enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_STATIC:
        return PARSER_TYPEMOD_STATIC;

    case LEXER_TOKENTYPE_CONSTEXPR:
        return PARSER_TYPEMOD_CONSTEXPR;

    default:
        assert(false);
    }
}

void Parser_Type_deinit(struct Parser_Type *self)
{
    gen_dyndeinit(&self->mods);
}

struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *end,
                                     struct DiagVec *diags)
{
    struct Parser_Type ret = {.mods = gen_dyninit()};

    isize_t i = start;

    while (Lexer_is_typemod(toks[i].type)) {
        gen_dynpush(&ret.mods, Parser_toktype_to_typemod(toks[i].type));
        ++i;
    }

    if (!Lexer_is_typespec(toks[i].type)) {
        struct Diag err = {.pos = toks[start].pos,
                           .line = toks[start].line,
                           .msg = strdup("expected a type specifier"),
                           .err = ERRORTYPE_MISSING_TYPESPEC,
                           .is_err = true};
        gen_dynpush(diags, err);
    } else {
        ret.spec = Parser_toktype_to_typespec(toks[i].type);
    }

    if (end)
        *end = i + 1;
    return ret;
}
