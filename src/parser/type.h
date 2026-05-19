#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"

struct Sema_Scope;

enum Parser_TypeSpec {
    PARSER_TYPESPEC_INVALID,

    // primitive types
    PARSER_TYPESPEC_VOID,
    PARSER_TYPESPEC_CHAR,
    PARSER_TYPESPEC_SCHAR,
    PARSER_TYPESPEC_UCHAR,
    PARSER_TYPESPEC_WCHAR,
    PARSER_TYPESPEC_CHAR16,
    PARSER_TYPESPEC_CHAR32,
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
    PARSER_TYPESPEC_BOOL,

    // the weird kids
    PARSER_TYPESPEC_FUNC,
    PARSER_TYPESPEC_FPTR,
    PARSER_TYPESPEC_ARRAY,
    PARSER_TYPESPEC_AUTO,
    PARSER_TYPESPEC_NULLPTR,

    // prefixed types
    PARSER_TYPESPEC_CLASS,
    PARSER_TYPESPEC_UNION,
    PARSER_TYPESPEC_ENUM,
};

bool Parser_is_typespec_named(enum Parser_TypeSpec spec);
enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type);
const char *Parser_typespec_to_str(enum Parser_TypeSpec spec);
bool Parser_is_integral_typespec(enum Parser_TypeSpec spec);
bool Parser_is_signed_integral_typespec(enum Parser_TypeSpec spec);
bool Parser_is_unsigned_integral_typespec(enum Parser_TypeSpec spec);
bool Parser_is_floating_typespec(enum Parser_TypeSpec spec);

struct Parser_TypeStorQual {
    bool is_static;
    bool is_constexpr;
    bool is_typedef;
};

// also called a CV-qualifier
struct Parser_TypeDataQual {
    bool is_const;
    bool is_volatile;
};
gen_dynarray_struct_named(Parser_TypeDataQualVec, struct Parser_TypeDataQual);

void Parser_set_squal_flag(struct Parser_TypeStorQual *qual,
                           enum Lexer_TokenType type);
void Parser_set_dqual_flag(struct Parser_TypeDataQual *qual,
                           enum Lexer_TokenType type);
isize_t Parser_parse_quals(const struct Lexer_Token *toks, isize_t start,
                           struct Parser_TypeStorQual *squals,
                           struct Parser_TypeDataQual *dquals);

struct Parser_TypeNamed {
    struct Sema_Scope *parent;
    i32 ident;
};

// not to be confused with an fptr. a func type can refer to multiple overloads
// of the same name, while a fptr refers to a specific overload without any
// specific name.
//
// example:
// int f(int, float);
// char *f(void *);
//
// // f is a func type, which gets cast to an fptr of signature
// // int (*)(int, float) as that is one of f's overloads
// int (*p)(int, float) = f;
struct Parser_TypeFunc {
    struct Sema_Scope *scope; // valid overloads are searched for from here
    const char *name;
    bool is_tor; // is a ctor or dtor
};

struct Sema_Ident *
Parser_named_type_ident(const struct Parser_TypeNamed *named);

struct Parser_Type {
    union {
        struct Parser_TypeNamed named; // used by classes, structs, enums, etc.
        struct Parser_TypeFunc func;
        struct Parser_TypeFPtr *fptr;
        struct Parser_TypeArray *array;
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
    bool has_ellipsis;
};

struct Parser_TypeArray {
    struct Parser_Type elem;
    u64 len;
};

struct Parser_ASTNode;

void Parser_Type_deinit(struct Parser_Type *self);
struct Parser_Type Parser_toktype_to_type(enum Lexer_TokenType type);
struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     struct Sema_Scope *scope,
                                     isize_t *out_declname,
                                     struct DiagVec *diags);
struct Parser_Type Parser_parse_base(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     struct Sema_Scope *scope,
                                     struct DiagVec *diags);
struct Parser_Type Parser_parse_type_no_base(const struct Lexer_Token *toks,
                                             isize_t start, isize_t *out_end,
                                             const struct Parser_Type *base,
                                             struct Sema_Scope *scope,
                                             isize_t *out_declname,
                                             struct DiagVec *diags);
isize_t Parser_n_indir(const struct Parser_Type *type);
struct Parser_Type Parser_copy_type(const struct Parser_Type *type);
struct Parser_TypeFPtr
Parser_copy_fptr_type(const struct Parser_TypeFPtr *fptr);
struct Parser_TypeArray
Parser_copy_array_type(const struct Parser_TypeArray *array);
struct Parser_Type Parser_ref_type(const struct Parser_Type *type,
                                   bool *out_failed);
struct Parser_Type Parser_deref_type(const struct Parser_Type *type,
                                     bool *out_failed);
char *Parser_type_to_str(const struct Parser_Type *type);
// can the token be the start of a type?
bool Parser_valid_type_start(const struct Lexer_Token *toks, isize_t idx,
                             const struct Sema_Scope *scope);
// the integral type specifier able to hold exactly the given number of bytes
enum Parser_TypeSpec Parser_uint_type_of_width(i32 bytes);
enum Parser_TypeSpec Parser_sint_type_of_width(i32 bytes);
i32 Parser_typespec_conv_rank(enum Parser_TypeSpec spec);
u64 Parser_integral_max(enum Parser_TypeSpec spec);
i64 Parser_integral_min(enum Parser_TypeSpec spec);
enum Parser_TypeSpec Parser_integral_prom(enum Parser_TypeSpec spec);
bool Parser_is_fundamental_type(const struct Parser_Type *type);
bool Parser_dquals_same(const struct Parser_TypeDataQual *a, isize_t n_a,
                        const struct Parser_TypeDataQual *b, isize_t n_b);
bool Parser_squals_same(const struct Parser_TypeStorQual *a,
                        const struct Parser_TypeStorQual *b);
bool Parser_are_types_same(const struct Parser_Type *a,
                           const struct Parser_Type *b);
struct Parser_Type Parser_create_func_type(struct Sema_Scope *scope,
                                           const char *name);
bool Parser_type_is_void_ptr(const struct Parser_Type *type);
bool Parser_type_is_nullptr_t(const struct Parser_Type *type);
