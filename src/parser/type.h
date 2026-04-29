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

    // the weird kids
    PARSER_TYPESPEC_FPTR,
    PARSER_TYPESPEC_ARRAY,
    PARSER_TYPESPEC_AUTO,

    // prefixed types
    PARSER_TYPESPEC_CLASS,
    PARSER_TYPESPEC_UNION,
    PARSER_TYPESPEC_ENUM,
};

bool Parser_is_typespec_named(enum Parser_TypeSpec spec);
enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type);

struct Parser_TypeStorQual {
    bool is_static;
    bool is_constexpr;
    bool is_typedef;
};

struct Parser_TypeDataQual {
    bool is_const;
};
gen_dynarray_struct_named(Parser_TypeDataQualVec, struct Parser_TypeDataQual);

struct Parser_Type {
    union {
        struct Parser_TypeFPtr *fptr;
        struct Parser_TypeArray *array;
        const char *named; // used by classes, structs, enums, etc.
    };
    struct Parser_TypeDataQualVec dquals; // one element for each level of
                                          // indirection, including 0.
                                          // starts at the top most ptr
    enum Parser_TypeSpec spec;
    struct Parser_TypeStorQual squals;
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

struct Parser_ASTNode;

void Parser_Type_deinit(struct Parser_Type *self);
// if the type is named then a name must be provided
struct Parser_Type Parser_toktype_to_type(enum Lexer_TokenType type,
                                          const char *name);
struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     const struct Parser_ASTNode *parent,
                                     const char **out_declname,
                                     struct DiagVec *diags);
isize_t Parser_n_indir(const struct Parser_Type *type);
struct Parser_Type Parser_copy_type(const struct Parser_Type *type);
struct Parser_Type Parser_ref_type(const struct Parser_Type *type,
                                   bool *out_failed);
struct Parser_Type Parser_deref_type(const struct Parser_Type *type,
                                     bool *out_failed);
