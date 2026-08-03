#pragma once

#include "apint.h"
#include "ints.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum midflt_IEEEKind {
    MIDFLT_IEEE_HALF,
    MIDFLT_IEEE_SINGLE,
    MIDFLT_IEEE_DOUBLE,
};

enum midflt_IEEERounding {
    MIDFLT_IEEE_ROUND_NEAREST_TIES_EVEN,
    MIDFLT_IEEE_ROUND_NEAREST_TIES_AWAY,
    MIDFLT_IEEE_ROUND_UP,
    MIDFLT_IEEE_ROUND_DOWN,
    MIDFLT_IEEE_ROUND_TOWARDS_ZERO,
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
    enum midflt_IEEERounding rounding; // current rounding mode
    enum midflt_IEEEValCat val_cat;    // value category

    bool is_neg; // sign bit
};

void midflt_IEEE_deinit(struct midflt_IEEE *self);
struct midflt_IEEE midflt_ieee_copy(const struct midflt_IEEE *src);
struct midflt_IEEE midflt_ieee_alloc(enum midflt_IEEEKind kind,
                                     enum midflt_IEEERounding rounding);
struct midflt_IEEE midflt_ieee_zero(bool is_neg, enum midflt_IEEEKind kind,
                                    enum midflt_IEEERounding rounding);
struct midflt_IEEE midflt_ieee_one(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_IEEERounding rounding);
struct midflt_IEEE midflt_ieee_inf(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_IEEERounding rounding);
struct midflt_IEEE midflt_ieee_nan(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_IEEERounding rounding);
// assumes the value category is MIDFLT_IEEE_VAL_NORMAL
struct midflt_IEEE midflt_ieee_init_manual(const struct mid_APInt *mant,
                                           i64 exp, bool is_neg,
                                           enum midflt_IEEEKind kind,
                                           enum midflt_IEEERounding rounding);
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
    MIDFLT_IEEE,
};

struct mid_APFloat {
    union {
        struct midflt_IEEE ieee;
    };
    enum midflt_Kind kind;
};

void midflt_deinit(struct mid_APFloat *self);

#ifdef __cplusplus
}
#endif
