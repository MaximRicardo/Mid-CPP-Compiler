#include "apfloat.h"
#include "apint.h"
#include "macros.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef enum midflt_IEEEKind IEEEKind;
typedef struct midflt_IEEE IEEE;

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

void midflt_ieee_mul(struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(a->kind == b->kind);

    // the sign bit is the XOR of the operands' sign bits
    a->is_neg = a->is_neg != b->is_neg;

    if (b->val_cat == MIDFLT_IEEE_VAL_ZERO) {
        a->exp = b->exp;
        midint_assign(&a->mant, &b->mant);
        return;
    } else if (a->val_cat == MIDFLT_IEEE_VAL_ZERO) {
        return;
    }

    a->exp += b->exp;

    // we need to be able to hold twice the width of the mantissa in case
    // the multiplication overflows
    auto tmp_res = midint_alloc(a->mant.n_bits * 2);
    midint_ufullmul(&a->mant, &b->mant, &tmp_res);

    // we need to shift the result back to account for all the trailing zeroes
    // in the multiplication
    i32 a_zeroes = midint_count_trailing_zeroes(&a->mant);
    i32 b_zeroes = midint_count_trailing_zeroes(&b->mant);
    i32 mul_shift = MID_MIN(a_zeroes, b_zeroes);
    midint_lshr_imm(&tmp_res, mul_shift);

    i32 tmp_bits = midint_unsigned_sig_bits(&tmp_res);
    assert(tmp_bits >= a->mant.n_bits);
    i32 norm_shift = tmp_bits - a->mant.n_bits;
    printf("shift = %d\n", norm_shift);
    printf("tmp bits = %d, mant bits = %d\n", tmp_bits, a->mant.n_bits);

    // if there was an overflow we shift the mantissa back to the decimal
    // point and move the extra amount to the exponent
    midint_lshr_imm(&tmp_res, norm_shift);
    a->exp += norm_shift - 1;

    midint_deinit(&a->mant);
    a->mant = tmp_res;
    midint_ext(&a->mant, b->mant.n_bits, false);
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
