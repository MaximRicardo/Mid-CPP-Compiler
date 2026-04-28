#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "vecs.h"

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
};

enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type);

struct Parser_TypeQual {
    bool is_static;
    bool is_constexpr;
};

struct Parser_Type {
    const char *name; // name of a class / struct / union, etc.
                      // for primitive types this is just the name of the type.
                      // NULL for ptrs
    enum Parser_TypeSpec spec;
    struct Parser_TypeQual quals;
    struct BoolVec is_const; // one element for each level of indirection
    bool is_lv_ref;
    bool is_rv_ref;
};

void Parser_Type_deinit(struct Parser_Type *self);
// end - an out parameter and can be NULL
struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *end,
                                     struct DiagVec *diags);
isize_t Parser_n_indir(const struct Parser_Type *type);
