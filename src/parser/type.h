#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "sema/ident.h"

struct MidSema_Scope;

enum MidParser_TypeSpec {
    MIDPARSER_TYPESPEC_INVALID,

    // primitive types
    MIDPARSER_TYPESPEC_VOID,
    MIDPARSER_TYPESPEC_CHAR,
    MIDPARSER_TYPESPEC_SCHAR,
    MIDPARSER_TYPESPEC_UCHAR,
    MIDPARSER_TYPESPEC_WCHAR,
    MIDPARSER_TYPESPEC_CHAR16,
    MIDPARSER_TYPESPEC_CHAR32,
    MIDPARSER_TYPESPEC_SHORT,
    MIDPARSER_TYPESPEC_USHORT,
    MIDPARSER_TYPESPEC_INT,
    MIDPARSER_TYPESPEC_UINT,
    MIDPARSER_TYPESPEC_LONG,
    MIDPARSER_TYPESPEC_ULONG,
    MIDPARSER_TYPESPEC_LONGLONG,
    MIDPARSER_TYPESPEC_ULONGLONG,
    MIDPARSER_TYPESPEC_FLOAT,
    MIDPARSER_TYPESPEC_DOUBLE,
    MIDPARSER_TYPESPEC_LONGDOUBLE,
    MIDPARSER_TYPESPEC_BOOL,

    // the weird kids
    MIDPARSER_TYPESPEC_FUNC,
    MIDPARSER_TYPESPEC_FPTR,
    MIDPARSER_TYPESPEC_ARRAY,
    MIDPARSER_TYPESPEC_AUTO,
    MIDPARSER_TYPESPEC_NULLPTR,
    MIDPARSER_TYPESPEC_TEMPLATED, // considered a named type, can't be
                                  // typechecked
    MIDPARSER_TYPESPEC_UNKNOWN,   // can't be type checked

    // prefixed types
    MIDPARSER_TYPESPEC_CLASS,
    MIDPARSER_TYPESPEC_UNION,
    MIDPARSER_TYPESPEC_ENUM,
};

bool MidParser_is_typespec_typecheckable(enum MidParser_TypeSpec spec);
bool MidParser_is_typespec_named(enum MidParser_TypeSpec spec);
enum MidParser_TypeSpec
MidParser_toktype_to_typespec(enum MidLexer_TokenType type);
const char *MidParser_typespec_to_str(enum MidParser_TypeSpec spec);
bool MidParser_is_integral_typespec(enum MidParser_TypeSpec spec);
bool MidParser_is_signed_integral_typespec(enum MidParser_TypeSpec spec);
bool MidParser_is_unsigned_integral_typespec(enum MidParser_TypeSpec spec);
bool MidParser_is_floating_typespec(enum MidParser_TypeSpec spec);

struct MidParser_TypeStorQual {
    bool is_static;
    bool is_constexpr;
    bool is_typedef;
};

// also called a CV-qualifier
struct MidParser_TypeDataQual {
    bool is_const;
    bool is_volatile;
};
MidGen_dynarray_struct_named(MidParser_TypeDataQualVec,
                             struct MidParser_TypeDataQual);

void MidParser_set_squal_flag(struct MidParser_TypeStorQual *qual,
                              enum MidLexer_TokenType type);
void MidParser_set_dqual_flag(struct MidParser_TypeDataQual *qual,
                              enum MidLexer_TokenType type);
mid_isize MidParser_parse_quals(const struct MidLexer_Token *toks,
                                mid_isize start,
                                struct MidParser_TypeStorQual *squals,
                                struct MidParser_TypeDataQual *dquals);

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
struct MidParser_TypeFunc {
    struct MidSema_Scope *scope; // valid overloads are searched for from here
    const char *name;
    bool is_tor; // is a ctor or dtor
};

struct MidParser_Type {
    union {
        struct MidSema_IdentPtr named; // used by classes, structs, enums, etc.
        struct MidParser_TypeFunc func;
        struct MidParser_TypeFPtr *fptr;
        struct MidParser_TypeArray *array;
    };
    struct MidParser_TypeDataQualVec dquals; // one element for each level of
                                             // indirection, including 0.
                                             // starts at the top most ptr
    enum MidParser_TypeSpec spec;
    struct MidParser_TypeStorQual squals;
    bool lv_ref;
    bool rv_ref;
};
MidGen_dynarray_struct_named(MidParser_TypeVec, struct MidParser_Type);

struct MidParser_TypeFPtr {
    struct MidParser_TypeVec params;
    struct MidParser_Type ret;
    bool has_ellipsis;
};

struct MidParser_TypeArray {
    struct MidParser_Type elem;
    u64 len;
};

struct MidParser_ASTNode;
struct MidParser_Allocators;

void MidParser_Type_deinit(struct MidParser_Type *self);
struct MidParser_Type MidParser_toktype_to_type(enum MidLexer_TokenType type);
struct MidParser_Type MidParser_parse_type(
    const struct MidLexer_Token *toks, mid_isize start, mid_isize *out_end,
    struct MidSema_Scope *scope, mid_isize *out_declname, bool is_type_id,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags);
struct MidParser_Type MidParser_parse_base(const struct MidLexer_Token *toks,
                                           mid_isize start, mid_isize *out_end,
                                           struct MidSema_Scope *scope,
                                           struct MidParser_Allocators *allocs,
                                           struct MidDiag_DiagVec *diags);
struct MidParser_Type MidParser_parse_type_no_base(
    const struct MidLexer_Token *toks, mid_isize start, mid_isize *out_end,
    const struct MidParser_Type *base, struct MidSema_Scope *scope,
    mid_isize *out_declname, bool is_type_id,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags);
mid_isize MidParser_n_indir(const struct MidParser_Type *type);
struct MidParser_Type MidParser_copy_type(const struct MidParser_Type *type);
struct MidParser_TypeFPtr
MidParser_copy_fptr_type(const struct MidParser_TypeFPtr *fptr);
struct MidParser_TypeArray
MidParser_copy_array_type(const struct MidParser_TypeArray *array);
struct MidParser_Type MidParser_ref_type(const struct MidParser_Type *type,
                                         bool *out_failed);
struct MidParser_Type MidParser_deref_type(const struct MidParser_Type *type,
                                           bool *out_failed);
char *MidParser_type_to_str(const struct MidParser_Type *type);
// can the token be the start of a type?
bool MidParser_valid_type_start(const struct MidLexer_Token *toks,
                                mid_isize idx,
                                const struct MidSema_Scope *scope);
// the integral type specifier able to hold exactly the given number of bytes
enum MidParser_TypeSpec MidParser_uint_type_of_width(i32 bytes);
enum MidParser_TypeSpec MidParser_sint_type_of_width(i32 bytes);
i32 MidParser_typespec_conv_rank(enum MidParser_TypeSpec spec);
u64 MidParser_integral_max(enum MidParser_TypeSpec spec);
i64 MidParser_integral_min(enum MidParser_TypeSpec spec);
enum MidParser_TypeSpec MidParser_integral_prom(enum MidParser_TypeSpec spec);
bool MidParser_is_fundamental_type(const struct MidParser_Type *type);
bool MidParser_dquals_same(const struct MidParser_TypeDataQual *a,
                           mid_isize n_a,
                           const struct MidParser_TypeDataQual *b,
                           mid_isize n_b);
bool MidParser_squals_same(const struct MidParser_TypeStorQual *a,
                           const struct MidParser_TypeStorQual *b);
bool MidParser_are_types_same(const struct MidParser_Type *a,
                              const struct MidParser_Type *b);
struct MidParser_Type MidParser_create_func_type(struct MidSema_Scope *scope,
                                                 const char *name);
struct MidParser_Type MidParser_create_named_type(struct MidSema_IdentPtr ident,
                                                  enum MidParser_TypeSpec spec);
struct MidParser_Type
MidParser_create_templated_type(struct MidSema_IdentPtr ident);
struct MidParser_Type MidParser_create_unknown_type();
bool MidParser_type_is_void(const struct MidParser_Type *type);
bool MidParser_type_is_void_ptr(const struct MidParser_Type *type);
bool MidParser_type_is_nullptr_t(const struct MidParser_Type *type);
bool MidParser_type_is_ref(const struct MidParser_Type *type);
// lvls of indir doesn't matter here
bool MidParser_type_is_typecheckable(const struct MidParser_Type *type);
