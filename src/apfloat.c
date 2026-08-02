#include "apfloat.h"
#include "apint.h"
#include "macros.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct midflt_IEEE IEEE;
typedef enum midflt_IEEEKind IEEEKind;
typedef enum midflt_IEEERounding IEEERounding;

void midflt_IEEE_deinit(struct midflt_IEEE *self)
{
    midint_deinit(&self->mant);
}

void midflt_deinit(struct mid_APFloat *self)
{
    switch (self->kind) {
    case MIDFLT_IEEE:
        midflt_IEEE_deinit(&self->ieee);
        break;

    default:
        MID_CRASH("invalid APFloat kind");
    }
}

static int ieee_exp_n_bits(IEEEKind kind)
{
    switch (kind) {
    case MIDFLT_IEEE_HALF:
        return 5;

    case MIDFLT_IEEE_SINGLE:
        return 8;

    case MIDFLT_IEEE_DOUBLE:
        return 11;

    default:
        MID_CRASH("ieee kind not supported");
    }
}

// includes the implicit one
static int ieee_mant_n_bits(IEEEKind kind)
{
    switch (kind) {
    case MIDFLT_IEEE_HALF:
        return 10 + 1;

    case MIDFLT_IEEE_SINGLE:
        return 23 + 1;

    case MIDFLT_IEEE_DOUBLE:
        return 52 + 1;

    default:
        MID_CRASH("ieee kind not supported");
    }
}

static i64 ieee_exp_max(IEEEKind kind)
{
    auto n_bits = ieee_exp_n_bits(kind);
    return (1ULL << (n_bits - 1)) - 1;
}

static i64 ieee_exp_min(IEEEKind kind)
{
    return 1 - ieee_exp_max(kind);
}

/*
static u64 ieee_biased_exp_max(IEEEKind kind)
{
    auto n_bits = ieee_exp_n_bits(kind);
    if (n_bits == 64)
        return UINT64_MAX;
    return (1ULL << n_bits) - 1;
}
*/

static u64 get_low_bits(u64 num, int bits)
{
    if (bits == 64)
        return num;

    u64 mask = (1ULL << bits) - 1;
    return num & mask;
}

static u64 ieee_bias_exp(i64 exp, IEEEKind kind)
{
    u64 bias = ieee_exp_max(kind);
    int n_bits = ieee_exp_n_bits(kind);
    return get_low_bits(exp + bias, n_bits);
}

static u64 ieee_biased_exp(const IEEE *self)
{
    return ieee_bias_exp(self->exp, self->kind);
}

struct midflt_IEEE midflt_ieee_alloc(enum midflt_IEEEKind kind,
                                     enum midflt_IEEERounding rounding)
{
    IEEE ret = {.kind = kind, .rounding = rounding};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_zero(bool is_neg, enum midflt_IEEEKind kind,
                                    enum midflt_IEEERounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_ZERO,
                .is_neg = is_neg};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_one(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_IEEERounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_NORMAL,
                .is_neg = is_neg};
    ret.mant = midint_zero(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_inf(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_IEEERounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_INF,
                .is_neg = is_neg};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_nan(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_IEEERounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_NAN,
                .is_neg = is_neg};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_init_manual(const struct mid_APInt *mant,
                                           i64 exp, bool is_neg,
                                           enum midflt_IEEEKind kind,
                                           enum midflt_IEEERounding rounding)
{
    assert(mant->n_bits == ieee_mant_n_bits(kind));
    assert(exp <= ieee_exp_max(kind));
    assert(exp >= ieee_exp_min(kind));

    if (!midint_get_sign_bit(mant))
        MID_CRASH("the MSb of the mantissa should always be active");

    IEEE ret = {.mant = midint_copy(mant),
                .exp = exp,
                .kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_NORMAL,
                .is_neg = is_neg};

    return ret;
}

void midflt_ieee_log(const struct midflt_IEEE *self, FILE *out)
{
    if (self->is_neg)
        fputc('-', out);

    // handle special cases
    if (self->val_cat == MIDFLT_IEEE_VAL_ZERO) {
        fputc('0', out);
        return;
    } else if (self->val_cat == MIDFLT_IEEE_VAL_INF) {
        fprintf(out, "inf");
        return;
    } else if (self->val_cat == MIDFLT_IEEE_VAL_NAN) {
        fprintf(out, "nan");
        return;
    }

    long double val = midflt_ieee_to_flt(self);
    fprintf(out, "%Lf", fabsl(val));
}

static bool ieee_should_inc_mant(IEEERounding mode, bool is_neg, bool guard,
                                 bool round_, bool sticky)
{
    switch (mode) {
    case MIDFLT_IEEE_ROUND_NEAREST_TIES_EVEN:
    case MIDFLT_IEEE_ROUND_NEAREST_TIES_AWAY:
        if (!guard)
            return false;
        else if (round_)
            return true;
        else if (sticky)
            return true;
        else
            return mode == MIDFLT_IEEE_ROUND_NEAREST_TIES_AWAY;

    case MIDFLT_IEEE_ROUND_UP:
        return !is_neg && (round_ || sticky);

    case MIDFLT_IEEE_ROUND_DOWN:
        return is_neg && (round_ || sticky);

    case MIDFLT_IEEE_ROUND_TOWARDS_ZERO:
        return false;

    default:
        MID_CRASH("invalid rounding mode");
    }
}

static bool ieee_muldiv_sign_bit(const IEEE *a, const IEEE *b)
{
    bool nan_arg =
        a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool both_nan =
        a->val_cat == MIDFLT_IEEE_VAL_NAN && b->val_cat == MIDFLT_IEEE_VAL_NAN;

    bool is_neg;
    // if only one of the operands is NaN, the sign of the NaN determines the
    // sign of the result
    if (!nan_arg || both_nan)
        // the sign bit is the XOR of the operands' sign bits
        is_neg = a->is_neg != b->is_neg;
    else if (b->val_cat == MIDFLT_IEEE_VAL_NAN)
        is_neg = b->is_neg;
    else
        is_neg = a->is_neg;

    return is_neg;
}

// handles special cases when multiplying.
// returns true if a special case was handled, false otherwise.
// if a special case was handled a is set to the result of the special
// case, otherwise a is unmodified.
static bool ieee_mul_special_cases(IEEE *a, const IEEE *b)
{
    bool inf_arg =
        a->val_cat == MIDFLT_IEEE_VAL_INF || b->val_cat == MIDFLT_IEEE_VAL_INF;
    bool nan_arg =
        a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool zero_arg = a->val_cat == MIDFLT_IEEE_VAL_ZERO ||
                    b->val_cat == MIDFLT_IEEE_VAL_ZERO;

    bool is_neg = ieee_muldiv_sign_bit(a, b);

    bool special_case = false;

    if (nan_arg) {
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        special_case = true;
    } else if (inf_arg && zero_arg) {
        // inf * 0 = nan
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        special_case = true;
    } else if (inf_arg) {
        // inf * x = inf if x is not 0 or NaN
        a->val_cat = MIDFLT_IEEE_VAL_INF;
        special_case = true;
    } else if (zero_arg) {
        // x * 0 = 0 if x is not inf or NaN
        a->val_cat = MIDFLT_IEEE_VAL_ZERO;
        special_case = true;
    }

    if (special_case)
        a->is_neg = is_neg;
    return special_case;
}

// handles special cases when dividing.
// returns true if a special case was handled, false otherwise.
// if a special case was handled a is set to the result of the special
// case, otherwise a is unmodified.
static bool ieee_div_special_cases(IEEE *a, const IEEE *b)
{
    bool a_inf = a->val_cat == MIDFLT_IEEE_VAL_INF;
    bool b_inf = b->val_cat == MIDFLT_IEEE_VAL_INF;
    bool a_nan = a->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool b_nan = b->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool a_zero = a->val_cat == MIDFLT_IEEE_VAL_ZERO;
    bool b_zero = b->val_cat == MIDFLT_IEEE_VAL_ZERO;

    bool is_neg = ieee_muldiv_sign_bit(a, b);

    bool special_case = false;

    if (a_nan || b_nan) {
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        special_case = true;
    } else if (a_inf && b_inf) {
        // inf / inf = -nan
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        is_neg = true;
        special_case = true;
    } else if (a_inf) {
        // inf / x = inf if x is not nan
        special_case = true;
    } else if (b_inf) {
        // x / inf = 0 if x is not nan
        a->val_cat = MIDFLT_IEEE_VAL_ZERO;
        special_case = true;
    } else if (a_zero && b_zero) {
        // 0 / 0 = -nan
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        is_neg = true;
        special_case = true;
    } else if (b_zero) {
        // x / 0 = inf if x is not nan or 0
        a->val_cat = MIDFLT_IEEE_VAL_INF;
        special_case = true;
    } else if (a_zero) {
        // 0 / x = 0 if x is not 0 or nan
        a->val_cat = MIDFLT_IEEE_VAL_ZERO;
        special_case = true;
    }

    if (special_case)
        a->is_neg = is_neg;
    return special_case;
}

static bool ieee_floats_compatible(const IEEE *a, const IEEE *b)
{
    return a->kind == b->kind && a->rounding == b->rounding;
}

void midflt_ieee_mul(struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (ieee_mul_special_cases(a, b))
        return;

    a->is_neg = ieee_muldiv_sign_bit(a, b);

    a->exp += b->exp;

    // we need to be able to hold twice the width of the mantissa in case
    // the multiplication overflows
    auto unnorm = midint_alloc(a->mant.n_bits * 2);
    midint_ufullmul(&a->mant, &b->mant, &unnorm);
    // account for the fact that the operands are implicitly multiplied
    // by 2 ^ (mant.n_bits - 1) for integer multiplication.
    midint_lshr_imm(&unnorm, a->mant.n_bits - 1);

    i32 bits = midint_unsigned_sig_bits(&unnorm);
    i32 norm_bits = a->mant.n_bits;
    assert(bits >= norm_bits);

    i32 norm_shift = bits - norm_bits;
    i32 zeroes = midint_count_trailing_zeroes(&unnorm);

    if (zeroes < norm_shift) {
        bool round_bit = midint_get_bit(&unnorm, norm_shift - 1);
        bool sticky_bit = zeroes < norm_shift - 1;
        bool guard_bit = midint_get_bit(&unnorm, zeroes);

        if (ieee_should_inc_mant(a->rounding, a->is_neg, guard_bit, round_bit,
                                 sticky_bit)) {
            midint_inc_bit(&unnorm, zeroes);
            // rounding might have introduced another significant bit
            bits = midint_unsigned_sig_bits(&unnorm);
            assert(bits >= norm_bits);
            norm_shift = bits - norm_bits;
        }
    }

    midint_lshr_imm(&unnorm, norm_shift);
    midint_deinit(&a->mant);
    a->mant = unnorm;
    midint_ext(&a->mant, norm_bits, false);

    a->exp += norm_shift;
}

void midflt_ieee_div(struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (ieee_div_special_cases(a, b))
        return;

    a->is_neg = ieee_muldiv_sign_bit(a, b);

    a->exp -= b->exp;

    // OPTIM: reuse a->mant for the remainder
    auto rem = midint_alloc(a->mant.n_bits);
    auto unnorm = midint_copy(&a->mant);
    // the division operation will shift all the bits to the right,
    // so we gotta put shift them all in front of the decimal point first.
    // note, we also shift by one extra bit to include rounding info
    midint_ext(&unnorm, a->mant.n_bits * 2, false);
    midint_shl_imm(&unnorm, a->mant.n_bits);
    midint_udivrem(&unnorm, &b->mant, &unnorm, &rem);
    bool round_bit = midint_get_bit(&unnorm, 0);
    midint_lshr_imm(&unnorm, 1);

    // if any of the remainder bits are active then one of the lost bits would
    // have been high if we we're dividing with infinite precision
    bool sticky_bit = midint_is_zero(&rem);

    bool guard_bit = midint_get_bit(&unnorm, 0);
    if (ieee_should_inc_mant(a->rounding, a->is_neg, guard_bit, round_bit,
                             sticky_bit))
        midint_inc_bit(&unnorm, 0);

    // normalize
    i32 n_bits = midint_unsigned_sig_bits(&unnorm);
    i32 norm_bits = a->mant.n_bits;
    if (n_bits > norm_bits) {
        auto shift = n_bits - norm_bits;
        a->exp += shift;
        midint_lshr_imm(&unnorm, shift);
    } else if (n_bits < norm_bits) {
        auto shift = norm_bits - n_bits;
        a->exp -= shift;
        midint_shl_imm(&unnorm, shift);
    }

    midint_deinit(&rem);
    midint_deinit(&a->mant);
    a->mant = unnorm;
    midint_ext(&a->mant, norm_bits, false);
}

#ifndef __STDC_IEC_60559_BFP__
// midflt_ieee_to_flt doesn't work otherwise rn
static_assert(false);
#endif

float ieee_to_single(const IEEE *self)
{
    union TypePun {
        u32 i;
        float f;
    } res;
    res.i = 0;

    u32 mant = midint_to_uint(&self->mant);
    // the MSb representing the implicit one needs to be masked away
    u32 mant_mask = (1ULL << (ieee_mant_n_bits(self->kind) - 1)) - 1;
    res.i |= mant & mant_mask;

    u32 exp = ieee_biased_exp(self);
    res.i |= exp << (ieee_mant_n_bits(self->kind) - 1);

    res.i |= (unsigned long long)self->is_neg << 31;

    return res.f;
}

long double midflt_ieee_to_flt(const struct midflt_IEEE *self)
{
    switch (self->kind) {
    case MIDFLT_IEEE_SINGLE:
        return ieee_to_single(self);

    default:
        MID_CRASH("converting this ieee kind is not supported");
    }
}
