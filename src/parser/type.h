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

    // the weird kids
    PARSER_TYPESPEC_FPTR,
    PARSER_TYPESPEC_ARRAY,
    PARSER_TYPESPEC_AUTO,

    // prefixed types
    PARSER_TYPESPEC_CLASS,
    PARSER_TYPESPEC_STRUCT,
    PARSER_TYPESPEC_UNION,
    PARSER_TYPESPEC_ENUM,
    PARSER_TYPESPEC_ENUMCLASS,
};

bool Parser_is_typespec_named(enum Parser_TypeSpec spec);
enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type);

struct Parser_TypeQual {
    bool is_static;
    bool is_constexpr;
};

struct Parser_Type {
    union {
        struct Parser_TypeFPtr *fptr;
        struct Parser_TypeArray *array;
        const char *named; // used by classes, structs, enums, etc.
    };
    struct BoolVec is_const; // one element for each level of indirection
    enum Parser_TypeSpec spec;
    struct Parser_TypeQual quals;
    bool lv_ref;
    bool rv_ref;
};
gen_dynarray_struct_named(Parser_TypeVec, struct Parser_Type);

struct Parser_TypeFPtr {
    struct Parser_TypeVec params;
    struct Parser_Type ret;
};

struct Parser_TypeArray {
    struct Parser_Type elem;
    isize_t len;
};

void Parser_Type_deinit(struct Parser_Type *self);
struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     const char **out_declname,
                                     struct DiagVec *diags);
isize_t Parser_n_indir(const struct Parser_Type *type);
