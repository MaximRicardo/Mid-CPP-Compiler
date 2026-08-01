#include "apfloat.h"
#include "apint.h"
#include "macros.h"

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
    }
}

static i64 ieee_exp_max(IEEEKind kind)
{
    auto n_bits = ieee_exp_n_bits(kind);
    return (1ULL << (n_bits - 1)) - 1;
}

static u64 ieee_biased_exp_max(IEEEKind kind)
{
    auto n_bits = ieee_exp_n_bits(kind);
    if (n_bits == 64)
        return UINT64_MAX;
    return (1ULL << n_bits) - 1;
}

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

// is the biased exponent all zeroes
static u64 ieee_exp_all_zeroes(const IEEE *self)
{
    return ieee_biased_exp(self) == 0;
}

// is the biased exponent all ones
static u64 ieee_exp_all_ones(const IEEE *self)
{
    return ieee_biased_exp(self) == ieee_biased_exp_max(self->kind);
}

bool midflt_IEEE_is_zero(const struct midflt_IEEE *self)
{
    return ieee_exp_all_zeroes(self) && midint_is_zero(&self->mant);
}

bool midflt_IEEE_is_inf(const struct midflt_IEEE *self)
{
    return ieee_exp_all_ones(self) && midint_is_zero(&self->mant);
}

bool midflt_IEEE_is_nan(const struct midflt_IEEE *self)
{
    return ieee_exp_all_ones(self) && !midint_is_zero(&self->mant);
}
