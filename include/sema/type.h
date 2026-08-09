#pragma once

#include "parser/type.h"

#ifdef __cplusplus
extern "C" {
#endif

bool midsema_is_typespec_typecheckable(enum midpar_TypeSpec spec);
bool midsema_is_typespec_named(enum midpar_TypeSpec spec);
enum midpar_TypeSpec midsema_toktype_to_typespec(enum midlex_TokenType type);
const char *midsema_typespec_to_str(enum midpar_TypeSpec spec);
bool midsema_is_integral_typespec(enum midpar_TypeSpec spec);
bool midsema_is_signed_integral_typespec(enum midpar_TypeSpec spec);
bool midsema_is_unsigned_integral_typespec(enum midpar_TypeSpec spec);
bool midsema_is_floating_typespec(enum midpar_TypeSpec spec);

mid_isize midsema_n_indir(const struct midpar_Type *type);
struct midpar_Type midsema_ref_type(const struct midpar_Type *type,
                                    bool *out_failed);
struct midpar_Type midsema_deref_type(const struct midpar_Type *type,
                                      bool *out_failed);

char *midsema_type_to_str(const struct midpar_Type *type);

bool midsema_type_is_scalar(const struct midpar_Type *type);
bool midsema_type_is_literal(const struct midpar_Type *type);
bool midsema_type_is_void(const struct midpar_Type *type);
bool midsema_type_is_void_ptr(const struct midpar_Type *type);
bool midsema_type_is_nullptr_t(const struct midpar_Type *type);
bool midsema_type_is_ref(const struct midpar_Type *type);
bool midsema_type_is_class_or_union(const struct midpar_Type *type);
bool midsema_type_is_array(const struct midpar_Type *type);
// lvls of indir doesn't matter here
bool midsema_type_is_typecheckable(const struct midpar_Type *type);
bool midsema_type_is_trivially_constructible(const struct midpar_Type *type);
bool midsema_type_has_trivial_default_ctor(const struct midpar_Type *type);
bool midsema_type_is_default_constructible(const struct midpar_Type *type);
bool midsema_type_is_constexpr_default_constructible(
    const struct midpar_Type *type);
bool midsema_dquals_same(const struct midpar_TypeDataQual *a, mid_isize n_a,
                         const struct midpar_TypeDataQual *b, mid_isize n_b);
bool midsema_squals_same(const struct midpar_TypeStorQual *a,
                         const struct midpar_TypeStorQual *b);
bool midsema_are_types_same(const struct midpar_Type *a,
                            const struct midpar_Type *b);

bool midsema_type_has_trivial_dtor(const struct midpar_Type *type);

// in bytes
int_least32_t midsema_typespec_size(enum midpar_TypeSpec spec);
// in bytes
struct mid_APInt midsema_type_size(const struct midpar_Type *type);
// in multiples of midtype_char_size
struct mid_APInt midsema_sizeof_type(const struct midpar_Type *type);

enum midflt_Kind midsema_get_flt_kind(enum midpar_TypeSpec spec);

enum midlit_ValueKind
midsema_type_lit_value_kind(const struct midpar_Type *type);

struct mid_APInt midsema_integral_max(enum midpar_TypeSpec spec);
struct mid_APInt midsema_integral_min(enum midpar_TypeSpec spec);

int32_t midsema_typespec_conv_rank(enum midpar_TypeSpec spec);
enum midpar_TypeSpec midsema_integral_prom(enum midpar_TypeSpec spec);

// returns true on success, returns false if the type isn't constexpr default
// constructible.
// out_val        - must not be NULL.
bool midsema_constexpr_default_init_type(const struct midpar_Type *type,
                                         struct midlit_TaggedValue *out_val);

#ifdef __cplusplus
}
#endif
