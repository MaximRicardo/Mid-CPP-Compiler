#pragma once

#include "apint.h"
#include "ints.h"
#include <stdio.h>

// TODO: add support for denormals in IEEE

#ifdef __cplusplus
extern "C" {
#endif

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
    i64 exp;               // exponent

    enum midflt_IEEEKind kind;
    enum midflt_Rounding rounding;  // current rounding mode
    enum midflt_IEEEValCat val_cat; // value category

    bool is_neg; // sign bit
};

void midflt_IEEE_deinit(struct midflt_IEEE *self);
struct midflt_IEEE midflt_ieee_init(double val, enum midflt_IEEEKind kind,
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
                                           i64 exp, bool is_neg,
                                           enum midflt_IEEEKind kind,
                                           enum midflt_Rounding rounding);
void midflt_ieee_log(const struct midflt_IEEE *self, FILE *out);

void midflt_ieee_add(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_sub(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_mul(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_div(struct midflt_IEEE *a, const struct midflt_IEEE *b);
void midflt_ieee_assign(struct midflt_IEEE *dest,
                        const struct midflt_IEEE *src);

bool midflt_ieee_eq(const struct midflt_IEEE *a, const struct midflt_IEEE *b);
bool midflt_ieee_gt(const struct midflt_IEEE *a, const struct midflt_IEEE *b);
bool midflt_ieee_gteq(const struct midflt_IEEE *a, const struct midflt_IEEE *b);
bool midflt_ieee_lt(const struct midflt_IEEE *a, const struct midflt_IEEE *b);
bool midflt_ieee_lteq(const struct midflt_IEEE *a, const struct midflt_IEEE *b);

double midflt_ieee_to_dbl(const struct midflt_IEEE *self);

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

bool midflt_compatible(const struct mid_APFloat *a,
                       const struct mid_APFloat *b);

void midflt_log(const struct mid_APFloat *self, FILE *out);
// prints a format string with APFloat arguments.
// example: midflt_print(stdout, "{} + {} is equal to {}\n", &a, &b, &sum);
// to print a curly bracket, use the "{{" and "}}" escape sequences.
// doesn't parse printf's '%' formats
void midflt_print(FILE *out, const char *restrict fmt, ...);

void midflt_add(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_sub(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_mul(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_div(struct mid_APFloat *a, const struct mid_APFloat *b);
void midflt_assign(struct mid_APFloat *a, const struct mid_APFloat *b);

bool midflt_eq(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_gt(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_gteq(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_lt(const struct mid_APFloat *a, const struct mid_APFloat *b);
bool midflt_lteq(const struct mid_APFloat *a, const struct mid_APFloat *b);

double midflt_to_dbl(const struct mid_APFloat *self);

#ifdef __cplusplus
}
#endif
