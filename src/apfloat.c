#include "apfloat.h"
#include "apint.h"
#include "macros.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct midflt_IEEE IEEE;
typedef enum midflt_IEEEKind IEEEKind;
typedef enum midflt_IEEEKind IEEERounding;

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

// NOTE: destroys unnorm
static void normalize_mant(IEEE *self, struct mid_APInt *unnorm)
{
    printf("unnorm = ");
    midint_log_hex(unnorm, stdout);
    printf("\n");

    i32 bits = midint_unsigned_sig_bits(unnorm);
    i32 norm_bits = self->mant.n_bits;
    assert(bits >= norm_bits);

    i32 norm_shift = bits - norm_bits;
    printf("norm shift = %d\n", norm_shift);

    i32 zeroes = midint_count_trailing_zeroes(unnorm);
    printf("zeroes = %d\n", zeroes);

    if (zeroes < norm_shift) {
        printf("rounding\n");
        // the most significant bit to be rounded away
        bool sigbit = midint_get_bit(unnorm, norm_shift - 1);
        // are we an equal distance away from the next and previous values?
        bool eq_dist = sigbit && zeroes == norm_shift - 1;

        switch (self->rounding) {
        case MIDFLT_IEEE_ROUND_TOWARDS_ZERO:
            // the bit shift already automatically rounds towards zero
            break;

        case MIDFLT_IEEE_ROUND_NEAREST_TIES_EVEN:
            if (eq_dist) {
                // round in whichever direction makes the normalized LSb a zero
                if (midint_get_bit(unnorm, zeroes))
                    midint_inc_bit(unnorm, zeroes);
            } else if (sigbit) {
                midint_inc_bit(unnorm, zeroes);
            }
            break;

        case MIDFLT_IEEE_ROUND_NEAREST_TIES_AWAY:
            if (sigbit)
                midint_inc_bit(unnorm, zeroes);
            break;

        case MIDFLT_IEEE_ROUND_UP:
            if (!self->is_neg)
                midint_inc_bit(unnorm, zeroes);
            break;

        case MIDFLT_IEEE_ROUND_DOWN:
            if (self->is_neg)
                midint_inc_bit(unnorm, zeroes);
            break;

        default:
            MID_CRASH("invalid rounding mode");
        }
    }

    midint_lshr_imm(unnorm, norm_shift);
    midint_deinit(&self->mant);
    self->mant = *unnorm;
    midint_ext(&self->mant, norm_bits, false);

    self->exp += norm_shift;
}

void midflt_ieee_mul(struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(a->kind == b->kind);

    // if one of the operands is NaN, the sign of the NaN determines the sign
    // of the result
    if (b->val_cat == MIDFLT_IEEE_VAL_NAN)
        a->is_neg = b->is_neg;
    else if (a->val_cat != MIDFLT_IEEE_VAL_NAN)
        // the sign bit is the XOR of the operands' sign bits
        a->is_neg = a->is_neg != b->is_neg;

    bool inf_arg =
        a->val_cat == MIDFLT_IEEE_VAL_INF || b->val_cat == MIDFLT_IEEE_VAL_INF;
    bool nan_arg =
        a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool zero_arg = a->val_cat == MIDFLT_IEEE_VAL_ZERO ||
                    b->val_cat == MIDFLT_IEEE_VAL_ZERO;

    // handle special cases
    if (nan_arg) {
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        return;
    } else if (inf_arg && zero_arg) {
        // inf * 0 = nan
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        return;
    } else if (inf_arg) {
        a->val_cat = MIDFLT_IEEE_VAL_INF;
        return;
    }

    a->exp += b->exp;

    // we need to be able to hold twice the width of the mantissa in case
    // the multiplication overflows
    auto unnorm = midint_alloc(a->mant.n_bits * 2);
    midint_ufullmul(&a->mant, &b->mant, &unnorm);
    // account for the fact that the operands are implicitly multiplied
    // by 2 ^ (mant.n_bits - 1) for integer multiplication.
    midint_lshr_imm(&unnorm, a->mant.n_bits - 1);

    normalize_mant(a, &unnorm);
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
