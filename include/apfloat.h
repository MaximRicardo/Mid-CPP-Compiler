#pragma once

#include "apint.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// NOTE: MUST BE CALLED BEFORE USING THIS MODULE
void midflt_init_module();

enum midflt_Rounding {
    MIDFLT_ROUND_NEAREST_TIES_EVEN,
    MIDFLT_ROUND_NEAREST_TIES_AWAY,
    MIDFLT_ROUND_UP,
    MIDFLT_ROUND_DOWN,
    MIDFLT_ROUND_TOWARDS_ZERO,
};

enum midflt_IEEEKind {
    MIDFLT_IEEE_HALF,
    MIDFLT_IEEE_SINGLE,
    MIDFLT_IEEE_DOUBLE,
};

// value category
enum midflt_IEEEValCat {
    MIDFLT_IEEE_VAL_ZERO,
    MIDFLT_IEEE_VAL_INF,
    MIDFLT_IEEE_VAL_NAN,
    MIDFLT_IEEE_VAL_NORMAL,
};

struct midflt_IEEE {
    struct mid_APInt mant; // mantissa (includes the implicit 1 in front of the
                           // decimal point)
    int64_t exp;           // unbiased exponent

    enum midflt_IEEEKind kind;
    enum midflt_Rounding rounding;  // current rounding mode
    enum midflt_IEEEValCat val_cat; // value category

    bool is_neg; // sign bit
};

void midflt_IEEE_deinit(struct midflt_IEEE *self);
struct midflt_IEEE midflt_ieee_init(double val, enum midflt_IEEEKind kind,
                                    enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_init_uint(const struct mid_APInt *val,
                                         enum midflt_IEEEKind kind,
                                         enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_init_sint(const struct mid_APInt *val,
                                         enum midflt_IEEEKind kind,
                                         enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_copy(const struct midflt_IEEE *src);
struct midflt_IEEE midflt_ieee_alloc(enum midflt_IEEEKind kind,
                                     enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_zero(bool is_neg, enum midflt_IEEEKind kind,
                                    enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_one(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_inf(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_Rounding rounding);
struct midflt_IEEE midflt_ieee_nan(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_Rounding rounding);
// assumes the value category is MIDFLT_IEEE_VAL_NORMAL
struct midflt_IEEE midflt_ieee_init_manual(const struct mid_APInt *mant,
                                           int64_t exp, bool is_neg,
                                           enum midflt_IEEEKind kind,
                                           enum midflt_Rounding rounding);
void midflt_ieee_print(const struct midflt_IEEE *self, FILE *out);
struct midflt_IEEE midflt_ieee_mantissa(const struct midflt_IEEE *self);

// in place operations
void midflt_ieee_add(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_sub(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_mul(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_div(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_assign(struct midflt_IEEE *dest,
                        const struct midflt_IEEE *src);
// n_iters      - the higher the better the precision
void midflt_ieee_approx_log2(struct midflt_IEEE *self, int n_iters);
// n_iters      - the higher the better the precision
void midflt_ieee_approx_ln(struct midflt_IEEE *self, int n_iters);
void midflt_ieee_log2(struct midflt_IEEE *self);
void midflt_ieee_log10(struct midflt_IEEE *self);
void midflt_ieee_ln(struct midflt_IEEE *self);
void midflt_ieee_flip_sign(struct midflt_IEEE *self);

// not in place operations
struct midflt_IEEE midflt_ieee_nip_add(const struct midflt_IEEE *a,
                                       const struct midflt_IEEE *b);
struct midflt_IEEE midflt_ieee_nip_sub(const struct midflt_IEEE *a,
                                       const struct midflt_IEEE *b);
struct midflt_IEEE midflt_ieee_nip_mul(const struct midflt_IEEE *a,
                                       const struct midflt_IEEE *b);
struct midflt_IEEE midflt_ieee_nip_div(const struct midflt_IEEE *a,
                                       const struct midflt_IEEE *b);
// n_iters      - the higher the better the precision
struct midflt_IEEE midflt_ieee_nip_approx_log2(const struct midflt_IEEE *self,
                                               int n_iters);
// n_iters      - the higher the better the precision
struct midflt_IEEE midflt_ieee_nip_approx_ln(const struct midflt_IEEE *self,
                                             int n_iters);
struct midflt_IEEE midflt_ieee_nip_log2(const struct midflt_IEEE *self);
struct midflt_IEEE midflt_ieee_nip_log10(const struct midflt_IEEE *self);
struct midflt_IEEE midflt_ieee_nip_ln(const struct midflt_IEEE *self);
struct midflt_IEEE midflt_ieee_nip_flip_sign(const struct midflt_IEEE *self);

bool midflt_ieee_is_eq(const struct midflt_IEEE *a,
                       const struct midflt_IEEE *b);
bool midflt_ieee_is_gt(const struct midflt_IEEE *a,
                       const struct midflt_IEEE *b);
bool midflt_ieee_is_gteq(const struct midflt_IEEE *a,
                         const struct midflt_IEEE *b);
bool midflt_ieee_is_lt(const struct midflt_IEEE *a,
                       const struct midflt_IEEE *b);
bool midflt_ieee_is_lteq(const struct midflt_IEEE *a,
                         const struct midflt_IEEE *b);
bool midflt_ieee_is_zero(const struct midflt_IEEE *self);
bool midflt_ieee_is_one(const struct midflt_IEEE *self);
bool midflt_ieee_is_minus_one(const struct midflt_IEEE *self);

double midflt_ieee_to_dbl(const struct midflt_IEEE *self);
// the width of the integer is the signed width required to represent both
// self's minimum and maximum value
struct mid_APInt midflt_ieee_to_sint(const struct midflt_IEEE *self);

void midflt_ieee_change_kind(struct midflt_IEEE *self,
                             enum midflt_IEEEKind new_kind);

enum midflt_Kind {
    MIDFLT_KIND_IEEE_START,
    MIDFLT_KIND_IEEE_HALF,
    MIDFLT_KIND_IEEE_SINGLE,
    MIDFLT_KIND_IEEE_DOUBLE,
    MIDFLT_KIND_IEEE_END,
};

static inline bool midflt_kind_is_ieee(enum midflt_Kind kind)
{
    return kind > MIDFLT_KIND_IEEE_START && kind < MIDFLT_KIND_IEEE_END;
}

struct mid_APFloat {
    union {
        struct midflt_IEEE ieee;
    };
    enum midflt_Kind kind;
};

void mid_APFloat_deinit(struct mid_APFloat *self);
struct mid_APFloat midflt_init(double val, enum midflt_Kind kind,
                               enum midflt_Rounding rounding);
struct mid_APFloat midflt_init_uint(const struct mid_APInt *val,
                                    enum midflt_Kind kind,
                                    enum midflt_Rounding rounding);
struct mid_APFloat midflt_init_sint(const struct mid_APInt *val,
                                    enum midflt_Kind kind,
                                    enum midflt_Rounding rounding);
struct mid_APFloat midflt_copy(const struct mid_APFloat *src);
enum midflt_Rounding midflt_get_rounding(const struct mid_APFloat *self);

bool midflt_compatible(const struct mid_APFloat *a,
                       const struct mid_APFloat *b);

void midflt_print(const struct mid_APFloat *self, FILE *out);

// in place operations
void midflt_add(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_sub(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_mul(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_div(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_assign(struct mid_APFloat *a, const struct mid_APFloat *b);
// n_iters      - the higher the better the precision
void midflt_approx_log2(struct mid_APFloat *self, int n_iters);
// n_iters      - the higher the better the precision
void midflt_approx_ln(struct mid_APFloat *self, int n_iters);
void midflt_log2(struct mid_APFloat *self);
void midflt_log10(struct mid_APFloat *self);
void midflt_ln(struct mid_APFloat *self);
void midflt_flip_sign(struct mid_APFloat *self);

// not in place operations
struct mid_APFloat midflt_nip_add(const struct mid_APFloat *a,
                                  const struct mid_APFloat *b);
struct mid_APFloat midflt_nip_sub(const struct mid_APFloat *a,
                                  const struct mid_APFloat *b);
struct mid_APFloat midflt_nip_mul(const struct mid_APFloat *a,
                                  const struct mid_APFloat *b);
struct mid_APFloat midflt_nip_div(const struct mid_APFloat *a,
                                  const struct mid_APFloat *b);
// n_iters      - the higher the better the precision
struct mid_APFloat midflt_nip_approx_log2(const struct mid_APFloat *self,
                                          int n_iters);
// n_iters      - the higher the better the precision
struct mid_APFloat midflt_nip_approx_ln(const struct mid_APFloat *self,
                                        int n_iters);
struct mid_APFloat midflt_nip_log2(const struct mid_APFloat *self);
struct mid_APFloat midflt_nip_log10(const struct mid_APFloat *self);
struct mid_APFloat midflt_nip_ln(const struct mid_APFloat *self);
struct mid_APFloat midflt_nip_flip_sign(const struct mid_APFloat *self);

bool midflt_is_eq(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_is_gt(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_is_gteq(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_is_lt(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_is_lteq(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_is_zero(const struct mid_APFloat *self);
bool midflt_is_one(const struct mid_APFloat *self);
bool midflt_is_minus_one(const struct mid_APFloat *self);

double midflt_to_dbl(const struct mid_APFloat *self);
struct mid_APInt midflt_to_sint(const struct mid_APFloat *self);

void midflt_change_kind(struct mid_APFloat *self, enum midflt_Kind new_kind);

#ifdef __cplusplus
}
#endif
