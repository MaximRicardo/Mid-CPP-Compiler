#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "sema/ident.h"

struct midsema_Scope;

enum midpar_TypeSpec {
    MIDPAR_TYPESPEC_INVALID,

    // primitive types
    MIDPAR_TYPESPEC_VOID,
    MIDPAR_TYPESPEC_CHAR,
    MIDPAR_TYPESPEC_SCHAR,
    MIDPAR_TYPESPEC_UCHAR,
    MIDPAR_TYPESPEC_WCHAR,
    MIDPAR_TYPESPEC_CHAR16,
    MIDPAR_TYPESPEC_CHAR32,
    MIDPAR_TYPESPEC_SHORT,
    MIDPAR_TYPESPEC_USHORT,
    MIDPAR_TYPESPEC_INT,
    MIDPAR_TYPESPEC_UINT,
    MIDPAR_TYPESPEC_LONG,
    MIDPAR_TYPESPEC_ULONG,
    MIDPAR_TYPESPEC_LONGLONG,
    MIDPAR_TYPESPEC_ULONGLONG,
    MIDPAR_TYPESPEC_FLOAT,
    MIDPAR_TYPESPEC_DOUBLE,
    MIDPAR_TYPESPEC_LONGDOUBLE,
    MIDPAR_TYPESPEC_BOOL,

    // the weird kids
    MIDPAR_TYPESPEC_FUNC,
    MIDPAR_TYPESPEC_FPTR,
    MIDPAR_TYPESPEC_ARRAY,
    MIDPAR_TYPESPEC_AUTO,
    MIDPAR_TYPESPEC_NULLPTR,
    MIDPAR_TYPESPEC_TEMPLATED, // considered a named type, can't be
                               // typechecked
    MIDPAR_TYPESPEC_UNKNOWN,   // can't be type checked

    // prefixed types
    MIDPAR_TYPESPEC_CLASS,
    MIDPAR_TYPESPEC_UNION,
    MIDPAR_TYPESPEC_ENUM,
};

bool midpar_is_typespec_typecheckable(enum midpar_TypeSpec spec);
bool midpar_is_typespec_named(enum midpar_TypeSpec spec);
enum midpar_TypeSpec midpar_toktype_to_typespec(enum midlex_TokenType type);
const char *midpar_typespec_to_str(enum midpar_TypeSpec spec);
bool midpar_is_integral_typespec(enum midpar_TypeSpec spec);
bool midpar_is_signed_integral_typespec(enum midpar_TypeSpec spec);
bool midpar_is_unsigned_integral_typespec(enum midpar_TypeSpec spec);
bool midpar_is_floating_typespec(enum midpar_TypeSpec spec);

struct midpar_TypeStorQual {
    bool is_static;
    bool is_constexpr;
    bool is_typedef;
};

// also called a CV-qualifier
struct midpar_TypeDataQual {
    bool is_const;
    bool is_volatile;
};
midgen_dynarray_struct_named(midpar_TypeDataQualVec,
                             struct midpar_TypeDataQual);

void midpar_set_squal_flag(struct midpar_TypeStorQual *qual,
                           enum midlex_TokenType type);
void midpar_set_dqual_flag(struct midpar_TypeDataQual *qual,
                           enum midlex_TokenType type);
mid_isize midpar_parse_quals(const struct midlex_Token *toks, mid_isize start,
                             struct midpar_TypeStorQual *squals,
                             struct midpar_TypeDataQual *dquals);

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
struct midpar_TypeFunc {
    struct midsema_Scope *scope; // valid overloads are searched for from here
    const char *name;
    bool is_tor; // is a ctor or dtor
};

struct midpar_Type {
    union {
        struct midsema_IdentPtr named; // used by classes, structs, enums, etc.
        struct midpar_TypeFunc func;
        struct midpar_TypeFPtr *fptr;
        struct midpar_TypeArray *array;
    };
    struct midpar_TypeDataQualVec dquals; // one element for each level of
                                          // indirection, including 0.
                                          // starts at the top most ptr
    enum midpar_TypeSpec spec;
    struct midpar_TypeStorQual squals;
    bool lv_ref;
    bool rv_ref;
};
midgen_dynarray_struct_named(midpar_TypeVec, struct midpar_Type);

struct midpar_TypeFPtr {
    struct midpar_TypeVec params;
    struct midpar_Type ret;
    bool has_ellipsis;
};

struct midpar_TypeArray {
    struct midpar_Type elem;
    u64 len;
};

struct midpar_ASTNode;
struct midpar_Allocators;

void midpar_Type_deinit(struct midpar_Type *self);
struct midpar_Type midpar_toktype_to_type(enum midlex_TokenType type);
struct midpar_Type midpar_parse_type(const struct midlex_Token *toks,
                                     mid_isize start, mid_isize *out_end,
                                     struct midsema_Scope *scope,
                                     mid_isize *out_declname, bool is_type_id,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags);
struct midpar_Type midpar_parse_base(const struct midlex_Token *toks,
                                     mid_isize start, mid_isize *out_end,
                                     struct midsema_Scope *scope,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags);
struct midpar_Type
midpar_parse_type_no_base(const struct midlex_Token *toks, mid_isize start,
                          mid_isize *out_end, const struct midpar_Type *base,
                          struct midsema_Scope *scope, mid_isize *out_declname,
                          bool is_type_id, struct midpar_Allocators *allocs,
                          struct mid_DiagVec *diags);
mid_isize midpar_n_indir(const struct midpar_Type *type);
struct midpar_Type midpar_copy_type(const struct midpar_Type *type);
struct midpar_TypeFPtr
midpar_copy_fptr_type(const struct midpar_TypeFPtr *fptr);
struct midpar_TypeArray
midpar_copy_array_type(const struct midpar_TypeArray *array);
struct midpar_Type midpar_ref_type(const struct midpar_Type *type,
                                   bool *out_failed);
struct midpar_Type midpar_deref_type(const struct midpar_Type *type,
                                     bool *out_failed);
char *midpar_type_to_str(const struct midpar_Type *type);
// can the token be the start of a type?
bool midpar_valid_type_start(const struct midlex_Token *toks, mid_isize idx,
                             const struct midsema_Scope *scope);
// the integral type specifier able to hold exactly the given number of bytes
enum midpar_TypeSpec midpar_uint_type_of_width(i32 bytes);
enum midpar_TypeSpec midpar_sint_type_of_width(i32 bytes);
i32 midpar_typespec_conv_rank(enum midpar_TypeSpec spec);
u64 midpar_integral_max(enum midpar_TypeSpec spec);
i64 midpar_integral_min(enum midpar_TypeSpec spec);
enum midpar_TypeSpec midpar_integral_prom(enum midpar_TypeSpec spec);
bool midpar_is_fundamental_type(const struct midpar_Type *type);
bool midpar_dquals_same(const struct midpar_TypeDataQual *a, mid_isize n_a,
                        const struct midpar_TypeDataQual *b, mid_isize n_b);
bool midpar_squals_same(const struct midpar_TypeStorQual *a,
                        const struct midpar_TypeStorQual *b);
bool midpar_are_types_same(const struct midpar_Type *a,
                           const struct midpar_Type *b);
struct midpar_Type midpar_create_func_type(struct midsema_Scope *scope,
                                           const char *name);
struct midpar_Type midpar_create_named_type(struct midsema_IdentPtr ident,
                                            enum midpar_TypeSpec spec);
struct midpar_Type midpar_create_templated_type(struct midsema_IdentPtr ident);
struct midpar_Type midpar_create_unknown_type();
bool midpar_type_is_void(const struct midpar_Type *type);
bool midpar_type_is_void_ptr(const struct midpar_Type *type);
bool midpar_type_is_nullptr_t(const struct midpar_Type *type);
bool midpar_type_is_ref(const struct midpar_Type *type);
// lvls of indir doesn't matter here
bool midpar_type_is_typecheckable(const struct midpar_Type *type);
