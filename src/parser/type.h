#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"

enum Parser_TypeSpec {
    // primitive types
    PARSER_TYPESPEC_VOID,
    PARSER_TYPESPEC_CHAR,
    PARSER_TYPESPEC_SCHAR,
    PARSER_TYPESPEC_UCHAR,
    PARSER_TYPESPEC_SHORT,
    PARSER_TYPESPEC_USHORT,
    PARSER_TYPESPEC_INT,
    PARSER_TYPESPEC_UINT,
    PARSER_TYPESPEC_LONG,
    PARSER_TYPESPEC_ULONG,
    PARSER_TYPESPEC_LONGLONG,
    PARSER_TYPESPEC_ULONGLONG,
    PARSER_TYPESPEC_FLOAT,
    PARSER_TYPESPEC_DOUBLE,
    PARSER_TYPESPEC_LONGDOUBLE,

    // the weird kid
    PARSER_TYPESPEC_AUTO,

    // prefixed types
    PARSER_TYPESPEC_STRUCT,
    PARSER_TYPESPEC_UNION,
    PARSER_TYPESPEC_ENUM,
    PARSER_TYPESPEC_ENUMCLASS,

    // custom types without a prefix
    PARSER_TYPESPEC_CUSTOM,
};

enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type);

enum Parser_TypeMod {
    PARSER_TYPEMOD_STATIC,
    PARSER_TYPEMOD_CONSTEXPR,
};
gen_dynarray_struct_named(Parser_TypeModVec, enum Parser_TypeMod);

enum Parser_TypeMod Parser_toktype_to_typemod(enum Lexer_TokenType type);

struct Parser_Type {
    enum Parser_TypeSpec spec;
    struct Parser_TypeModVec mods;
};

void Parser_Type_deinit(struct Parser_Type *self);
// end - an out parameter and can be NULL
struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *end,
                                     struct DiagVec *diags);
