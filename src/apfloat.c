#include "apfloat.h"
#include "apint.h"
#include "macros.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct midflt_IEEE IEEE;
typedef enum midflt_IEEEKind IEEEKind;
typedef enum midflt_Rounding Rounding;

void midflt_IEEE_deinit(struct midflt_IEEE *self)
{
    mid_APInt_deinit(&self->mant);
}

struct midflt_IEEE midflt_ieee_copy(const struct midflt_IEEE *src)
{
    struct midflt_IEEE ret = {.mant = midint_copy(&src->mant),
                              .exp = src->exp,
                              .kind = src->kind,
                              .rounding = src->rounding,
                              .val_cat = src->val_cat,
                              .is_neg = src->is_neg};
    return ret;
}

void mid_APFloat_deinit(struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        midflt_IEEE_deinit(&self->ieee);
    else
        MID_CRASH("invalid APFloat kind");
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
*/

struct midflt_IEEE midflt_ieee_init(double val, enum midflt_IEEEKind kind,
                                    enum midflt_Rounding rounding)
{
    auto ret = midflt_ieee_one(signbit(val), kind, rounding);

    if (isnan(val)) {
        ret.val_cat = MIDFLT_IEEE_VAL_NAN;
        return ret;
    } else if (isinf(val)) {
        ret.val_cat = MIDFLT_IEEE_VAL_INF;
        return ret;
    } else if (iszero(val)) {
        ret.val_cat = MIDFLT_IEEE_VAL_ZERO;
        return ret;
    }

    ret.exp = floor(log2(fabs(val)));
    assert(ret.exp <= ieee_exp_max(ret.kind));
    assert(ret.exp >= ieee_exp_min(ret.kind));

    double mant = fabs(val) * exp2(-ret.exp);
    assert(mant >= 1.0 && mant < 2.0);

    double digit_val = 0.5;
    for (i32 i = ret.mant.n_bits - 2; i >= 0; --i, digit_val /= 2.0) {
        if (mant - 1.0 >= digit_val) {
            midint_flip_bit(&ret.mant, i);
            mant -= digit_val;
        }

        if (digit_val < DBL_TRUE_MIN * 2.0)
            break;
    }

    return ret;
}

struct midflt_IEEE midflt_ieee_alloc(enum midflt_IEEEKind kind,
                                     enum midflt_Rounding rounding)
{
    IEEE ret = {.kind = kind, .rounding = rounding};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_zero(bool is_neg, enum midflt_IEEEKind kind,
                                    enum midflt_Rounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_ZERO,
                .is_neg = is_neg};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_one(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_Rounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_NORMAL,
                .is_neg = is_neg};
    ret.mant = midint_zero(ieee_mant_n_bits(ret.kind));
    midint_flip_bit(&ret.mant, ret.mant.n_bits - 1);

    return ret;
}

struct midflt_IEEE midflt_ieee_inf(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_Rounding rounding)
{
    IEEE ret = {.kind = kind,
                .rounding = rounding,
                .val_cat = MIDFLT_IEEE_VAL_INF,
                .is_neg = is_neg};
    ret.mant = midint_alloc(ieee_mant_n_bits(ret.kind));

    return ret;
}

struct midflt_IEEE midflt_ieee_nan(bool is_neg, enum midflt_IEEEKind kind,
                                   enum midflt_Rounding rounding)
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
                                           enum midflt_Rounding rounding)
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

    // TODO: make this not rely on double cuz it WILL NOT WORK if the internal
    //       float is bigger than a double
    double val = midflt_ieee_to_dbl(self);
    fprintf(out, "%.*lf", DECIMAL_DIG, fabs(val));
}

static bool ieee_should_inc_mant(Rounding mode, bool is_neg, bool guard,
                                 bool round_, bool sticky)
{
    switch (mode) {
    case MIDFLT_ROUND_NEAREST_TIES_EVEN:
        if (!round_)
            // round_ marks the halfway point. if it's off then we're guaranteed
            // to be closer to the lower number than the upper one
            return false;
        else
            // if sticky is high we always round up as we're closer to the next
            // number.
            // if guard is high then we also round up cuz rounding up takes
            // priority over rounding down
            return guard || sticky;

    case MIDFLT_ROUND_NEAREST_TIES_AWAY:
        // since round_ marks the halfway point between the next and previous
        // value, and rounding up always takes priority regardless of the guard
        // bit, all we need to know is if we're at the half way point or past it
        return round_;

    case MIDFLT_ROUND_UP:
        return !is_neg && (round_ || sticky);

    case MIDFLT_ROUND_DOWN:
        return is_neg && (round_ || sticky);

    case MIDFLT_ROUND_TOWARDS_ZERO:
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

    // TODO: reuse ieee_round_extra_mant_bits here for normalizing the result

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
        bool guard_bit = midint_get_bit(&unnorm, norm_shift);

        if (ieee_should_inc_mant(a->rounding, a->is_neg, guard_bit, round_bit,
                                 sticky_bit)) {
            // allocate an extra bit for carry
            midint_ext(&unnorm, unnorm.n_bits + 1, false);
            midint_inc_bit(&unnorm, norm_shift);
            // rounding might have introduced another significant bit
            bits = midint_unsigned_sig_bits(&unnorm);
            assert(bits >= norm_bits);
            norm_shift = bits - norm_bits;
        }
    }

    midint_lshr_imm(&unnorm, norm_shift);
    mid_APInt_deinit(&a->mant);
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

    // OPTIM: reuse a->mant for the remainder, tho btw that wouldn't work if
    //        a and b alias
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

    mid_APInt_deinit(&rem);
    mid_APInt_deinit(&a->mant);
    a->mant = unnorm;
    midint_ext(&a->mant, norm_bits, false);
}

static void ieee_addsub_normalize_mant(IEEE *a)
{
    i32 unnorm_bits = midint_unsigned_sig_bits(&a->mant);
    i32 norm_bits = ieee_mant_n_bits(a->kind);

    if (unnorm_bits > norm_bits) {
        i32 shift = unnorm_bits - norm_bits;

        bool guard_bit = midint_get_bit(&a->mant, shift);
        bool rounding_bit = midint_get_bit(&a->mant, shift - 1);
        i32 zeroes = midint_count_trailing_zeroes(&a->mant);
        bool sticky_bit = zeroes < shift - 1;

        if (ieee_should_inc_mant(a->rounding, a->is_neg, guard_bit,
                                 rounding_bit, sticky_bit)) {
            // allocate an extra bit for carry
            midint_ext(&a->mant, a->mant.n_bits + 1, false);
            midint_inc_bit(&a->mant, shift);
            unnorm_bits = midint_unsigned_sig_bits(&a->mant);
            shift = unnorm_bits - norm_bits;
        }

        midint_lshr_imm(&a->mant, shift);
        midint_ext(&a->mant, norm_bits, false);
        a->exp += shift;
    } else if (unnorm_bits < norm_bits) {
        i32 shift = norm_bits - unnorm_bits;
        midint_shl_imm(&a->mant, shift);
        a->exp -= shift;
    }
}

// automatically resizes the mantissa APInts to hold the extra bits required
// to match the exponents as well as extra_bits
static void ieee_match_exps(IEEE *a, IEEE *b, i32 extra_bits)
{
    if (a->exp > b->exp) {
        auto shift = a->exp - b->exp;

        midint_ext(&a->mant, a->mant.n_bits + shift + extra_bits, false);
        midint_ext(&b->mant, b->mant.n_bits + shift + extra_bits, false);
        midint_shl_imm(&a->mant, shift);
        a->exp = b->exp;
    } else if (b->exp > a->exp) {
        auto shift = b->exp - a->exp;

        midint_ext(&a->mant, a->mant.n_bits + shift + extra_bits, false);
        midint_ext(&b->mant, b->mant.n_bits + shift + extra_bits, false);
        midint_shl_imm(&b->mant, shift);
        b->exp = a->exp;
    } else if (extra_bits != 0) {
        midint_ext(&a->mant, a->mant.n_bits + extra_bits, false);
        midint_ext(&b->mant, b->mant.n_bits + extra_bits, false);
    }
}

// does no sign checks and doesnt detect special cases
static void ieee_add_base(IEEE *a, const IEEE *og_b)
{
    assert(ieee_floats_compatible(a, og_b));

    auto b = midflt_ieee_copy(og_b);

    // reserve an extra bit for carry information
    ieee_match_exps(a, &b, 1);

    midint_add(&a->mant, &b.mant);

    ieee_addsub_normalize_mant(a);

    midint_ext(&a->mant, ieee_mant_n_bits(a->kind), false);
    midflt_IEEE_deinit(&b);
}

// does no sign checks and doesnt detect special cases
static void ieee_sub_base(IEEE *a, const IEEE *og_b)
{
    assert(ieee_floats_compatible(a, og_b));

    auto b = midflt_ieee_copy(og_b);

    // reserve an extra bit for borrow information
    ieee_match_exps(a, &b, 1);

    midint_sub(&a->mant, &b.mant);

    ieee_addsub_normalize_mant(a);

    midint_ext(&a->mant, ieee_mant_n_bits(a->kind), false);
    midflt_IEEE_deinit(&b);
}

// handles special cases when adding or subtracting.
// returns true if a special case was handled, false otherwise.
// if a special case was handled a is set to the result of the special
// case, otherwise a is unmodified.
// sub        - is this a subtraction or a multiplication?
static bool ieee_addsub_special_cases(IEEE *a, const IEEE *b, bool sub)
{
    bool b_neg = sub ? b->is_neg : !b->is_neg;

    bool inf_arg =
        a->val_cat == MIDFLT_IEEE_VAL_INF || b->val_cat == MIDFLT_IEEE_VAL_INF;
    bool both_inf =
        a->val_cat == MIDFLT_IEEE_VAL_INF && b->val_cat == MIDFLT_IEEE_VAL_INF;
    bool nan_arg =
        a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool both_nan =
        a->val_cat == MIDFLT_IEEE_VAL_NAN && b->val_cat == MIDFLT_IEEE_VAL_NAN;
    bool zero_arg = a->val_cat == MIDFLT_IEEE_VAL_ZERO ||
                    b->val_cat == MIDFLT_IEEE_VAL_ZERO;
    bool both_zero = a->val_cat == MIDFLT_IEEE_VAL_ZERO &&
                     b->val_cat == MIDFLT_IEEE_VAL_ZERO;

    if (both_nan) {
        // nan + nan = either -nan if both operands are negative or nan if not
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        a->is_neg = a->is_neg && a->is_neg == b_neg;
    } else if (nan_arg) {
        // nan + x = +nan
        a->val_cat = MIDFLT_IEEE_VAL_NAN;
        a->is_neg = false;
    } else if (both_inf) {
        // inf + inf = either +/-inf if the signs match or -nan if not
        if (a->is_neg != b_neg) {
            a->val_cat = MIDFLT_IEEE_VAL_NAN;
            a->is_neg = true;
        }
    } else if (inf_arg) {
        // inf + x = inf if x is not nan
        a->val_cat = MIDFLT_IEEE_VAL_INF;
    } else if (both_zero) {
        // 0 + 0 = is either -0 if both operands are negative or 0 otherwise
        a->is_neg = a->is_neg && b_neg;
    } else if (zero_arg) {
        // x + 0 = x if x is not nan
        if (a->val_cat == MIDFLT_IEEE_VAL_ZERO) {
            midflt_ieee_assign(a, b);
            a->is_neg = b_neg;
        }
    } else if (sub && midflt_ieee_eq(a, b)) {
        // x - x = +0 if x is a normal value or zero
        a->val_cat = MIDFLT_IEEE_VAL_ZERO;
        // rounding down causes x - x to be equal to -0 instead of 0
        a->is_neg = b->rounding == MIDFLT_ROUND_DOWN;
    } else {
        return false;
    }

    return true;
}

void midflt_ieee_add(struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (ieee_addsub_special_cases(a, b, false))
        return;

    if (a == b) {
        ++a->exp;
        return;
    }

    // account for going above or below zero
    if (a->is_neg && !b->is_neg) {
        a->is_neg = false; // temporarily flip the sign of a to cmp magnitudes
        if (midflt_ieee_lt(a, b)) {
            // -a + +b == +b - +a
            auto tmp_b = midflt_ieee_copy(b);
            ieee_sub_base(&tmp_b, a);

            midflt_IEEE_deinit(a);
            *a = tmp_b;

            return;
        }
        a->is_neg = true;
    }

    if (b->is_neg) {
        auto tmp_b = *b;
        tmp_b.is_neg = false;
        midflt_ieee_sub(a, &tmp_b);
    } else {
        ieee_add_base(a, b);
    }
}

void midflt_ieee_sub(struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (a == b) {
        if (a->val_cat == MIDFLT_IEEE_VAL_NORMAL) {
            a->val_cat = MIDFLT_IEEE_VAL_ZERO;
            a->is_neg = false;
        }
        return;
    }

    if (ieee_addsub_special_cases(a, b, true))
        return;

    // account for going above or below zero
    if (!a->is_neg && !b->is_neg && midflt_ieee_lt(a, b)) {
        // +a - +b == -(+b - +a)
        auto tmp_b = midflt_ieee_copy(b);
        ieee_sub_base(&tmp_b, a);

        midflt_IEEE_deinit(a);
        *a = tmp_b;
        a->is_neg = true;
    } else if (a->is_neg && b->is_neg && midflt_ieee_gt(a, b)) {
        // -a - -b == +b - +a
        auto tmp_b = midflt_ieee_copy(b);
        tmp_b.is_neg = false;
        a->is_neg = false;
        ieee_sub_base(&tmp_b, a);

        midflt_IEEE_deinit(a);
        *a = tmp_b;
    } else if (a->is_neg && !b->is_neg) {
        // -a - +b == -(+b - -a) == -(+b + +a) == -(+a + +b)
        a->is_neg = false;
        ieee_add_base(a, b);
        a->is_neg = true;
    } else if (!a->is_neg && b->is_neg) {
        auto tmp_b = *b;
        tmp_b.is_neg = false;
        midflt_ieee_add(a, &tmp_b);
    } else {
        ieee_sub_base(a, b);
    }
}

double midflt_ieee_to_dbl(const struct midflt_IEEE *self)
{
    if (self->val_cat == MIDFLT_IEEE_VAL_NAN)
        // 0 / 0 = -nan
        return self->is_neg ? 0.0 / 0.0 : -(0.0 / 0.0);
    else if (self->val_cat == MIDFLT_IEEE_VAL_INF)
        // 1 / 0 = inf
        return self->is_neg ? -(1.0 / 0.0) : 1.0 / 0.0;
    else if (self->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return self->is_neg ? -0.0 : 0.0;

    // these assertions rely on the radix being the same base as the internal
    // float, which is base 2
    static_assert(FLT_RADIX == 2);
    assert(self->exp <= DBL_MAX_EXP);
    assert(self->exp >= DBL_MIN_EXP);

    double res = pow(2.0, self->exp);

    double mant = 1.0;
    double inc = 0.5;
    // skip the implicit leading 1
    for (i32 i = self->mant.n_bits - 2; i >= 0; --i) {
        if (midint_get_bit(&self->mant, i))
            mant += inc;

        // no point looping past the precision of double
        if (inc < DBL_TRUE_MIN * 2.0)
            break;
        inc /= 2.0;
    }

    res *= mant;
    return res;
}

void midflt_ieee_assign(struct midflt_IEEE *dest, const struct midflt_IEEE *src)
{
    assert(ieee_floats_compatible(dest, src));

    dest->val_cat = src->val_cat;
    dest->exp = src->exp;
    midint_assign(&dest->mant, &src->mant);
}

bool midflt_ieee_eq(const struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN)
        return false;

    if (a->val_cat == MIDFLT_IEEE_VAL_ZERO &&
        b->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return true;

    if (a->is_neg != b->is_neg)
        return false;

    if (a->val_cat != b->val_cat)
        return false;

    if (a->val_cat == MIDFLT_IEEE_VAL_NORMAL)
        return a->exp == b->exp && midint_is_eq(&a->mant, &b->mant);

    return true;
}

bool midflt_ieee_gt(const struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN)
        return false;

    if (a->val_cat == MIDFLT_IEEE_VAL_ZERO &&
        b->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return false;
    else if (a->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return b->is_neg; // 0 > +x == false and 0 > -x == true
    else if (b->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return !a->is_neg; // +x > 0 == true and -x > 0 == false

    if (a->is_neg != b->is_neg)
        return b->is_neg; // +x > -y == true and -x > +y == false

    if (a->val_cat == MIDFLT_IEEE_VAL_INF)
        return !a->is_neg; // inf > x == true and -inf > x == false
    else if (b->val_cat == MIDFLT_IEEE_VAL_INF)
        return b->is_neg; // x > inf == false and x > -inf == true

    // both operands are normal values

    if (a->exp != b->exp)
        return a->is_neg ? a->exp < b->exp : a->exp > b->exp;

    int cmp = midint_unsigned_cmp(&a->mant, &b->mant);
    if (cmp == 0)
        return false;
    else if (cmp > 0)
        return !a->is_neg;
    else
        return a->is_neg;
}

bool midflt_ieee_gteq(const struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN)
        return false;

    return !midflt_ieee_lt(a, b);
}

bool midflt_ieee_lt(const struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN)
        return false;

    if (a->val_cat == MIDFLT_IEEE_VAL_ZERO &&
        b->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return false;
    else if (a->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return !b->is_neg; // 0 < +x == true and 0 < -x == false
    else if (b->val_cat == MIDFLT_IEEE_VAL_ZERO)
        return a->is_neg; // +x < 0 == false and -x < 0 == true

    if (a->is_neg != b->is_neg)
        return a->is_neg; // +x < -y == false and -x < +y == true

    if (a->val_cat == MIDFLT_IEEE_VAL_INF)
        return a->is_neg; // inf < x == false and -inf < x == true
    else if (b->val_cat == MIDFLT_IEEE_VAL_INF)
        return !b->is_neg; // x < inf == true and x < -inf == false

    // both operands are normal values

    if (a->exp != b->exp)
        return !a->is_neg ? a->exp < b->exp : a->exp > b->exp;

    int cmp = midint_unsigned_cmp(&a->mant, &b->mant);
    if (cmp == 0)
        return false;
    else if (cmp < 0)
        return !a->is_neg;
    else
        return a->is_neg;
}

bool midflt_ieee_lteq(const struct midflt_IEEE *a, const struct midflt_IEEE *b)
{
    assert(ieee_floats_compatible(a, b));

    if (a->val_cat == MIDFLT_IEEE_VAL_NAN || b->val_cat == MIDFLT_IEEE_VAL_NAN)
        return false;

    return !midflt_ieee_gt(a, b);
}

bool midflt_ieee_is_zero(const struct midflt_IEEE *self)
{
    return self->val_cat == MIDFLT_IEEE_VAL_ZERO;
}

static bool ieee_is_one(const IEEE *self, bool desired_sign)
{
    if (self->val_cat != MIDFLT_IEEE_VAL_NORMAL)
        return false;
    if (self->exp != 0)
        return false;
    if (self->is_neg != desired_sign)
        return false;

    // check if only the MSb or the "sign bit" is active
    return midint_is_smin(&self->mant);
}

bool midflt_ieee_is_one(const struct midflt_IEEE *self)
{
    return ieee_is_one(self, false);
}

bool midflt_ieee_is_minus_one(const struct midflt_IEEE *self)
{
    return ieee_is_one(self, true);
}

bool midflt_ieee_is_minus_one(const struct midflt_IEEE *self);

static IEEEKind kind_to_ieee_kind(enum midflt_Kind kind)
{
    switch (kind) {
    case MIDFLT_KIND_IEEE_HALF:
        return MIDFLT_IEEE_HALF;

    case MIDFLT_KIND_IEEE_SINGLE:
        return MIDFLT_IEEE_SINGLE;

    case MIDFLT_KIND_IEEE_DOUBLE:
        return MIDFLT_IEEE_DOUBLE;

    default:
        MID_CRASH("kind is not an IEEE float");
    }
}

struct mid_APFloat midflt_init(double val, enum midflt_Kind kind,
                               enum midflt_Rounding rounding)
{
    struct mid_APFloat ret = {.kind = kind};

    if (midflt_kind_is_ieee(ret.kind))
        ret.ieee = midflt_ieee_init(val, kind_to_ieee_kind(ret.kind), rounding);
    else
        MID_CRASH("unsupported APFloat kind");

    return ret;
}

bool midflt_compatible(const struct mid_APFloat *a, const struct mid_APFloat *b)
{
    if (a->kind != b->kind)
        return false;

    if (midflt_kind_is_ieee(a->kind))
        return ieee_floats_compatible(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_add(struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        midflt_ieee_add(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_sub(struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        midflt_ieee_sub(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_mul(struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        midflt_ieee_mul(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_div(struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        midflt_ieee_div(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_assign(struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        midflt_ieee_assign(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_eq(const struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        return midflt_ieee_eq(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_gt(const struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        return midflt_ieee_gt(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_gteq(const struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        return midflt_ieee_gteq(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_lt(const struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        return midflt_ieee_lt(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_lteq(const struct mid_APFloat *a, const struct mid_APFloat *b)
{
    assert(midflt_compatible(a, b));

    if (midflt_kind_is_ieee(a->kind))
        return midflt_ieee_lteq(&a->ieee, &b->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

double midflt_to_dbl(const struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        return midflt_ieee_to_dbl(&self->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_log(const struct mid_APFloat *self, FILE *out)
{
    if (midflt_kind_is_ieee(self->kind))
        midflt_ieee_log(&self->ieee, out);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_print(FILE *out, const char *restrict fmt, ...)
{
    va_list args;
    va_start(args);

    char c;
    while ((c = *(fmt++)) != '\0') {
        if (c == '{') {
            c = *(fmt++);
            if (c == '{') {
                fputc('{', out);
            } else if (c == '}') {
                auto val = va_arg(args, const struct mid_APFloat *);
                midflt_log(val, out);
            } else {
                MID_CRASH(
                    "expected a closing curly bracket in the format string");
            }
        } else if (c == '}') {
            c = *(fmt++);
            if (c == '}')
                fputc('}', out);
            else
                MID_CRASH(
                    "extraneous closing curly bracket in the format string");
        } else {
            fputc(c, out);
        }
    }

    va_end(args);
}

struct midflt_IEEE midflt_ieee_mantissa(const struct midflt_IEEE *self)
{
    // self = 2^n * m
    // mant = self * 2^-n = 2^n * m * 2^-n = m

    auto res = midflt_ieee_one(false, self->kind, self->rounding);
    res.exp = -self->exp;

    midflt_ieee_mul(&res, self);
    return res;
}

static u32 ieee_log2_step(IEEE *self)
{
    u32 m = 0;
    auto two = midflt_ieee_init(2.0, self->kind, self->rounding);

    while (midflt_ieee_lt(self, &two)) {
        midflt_ieee_mul(self, self);
        ++m;
    }

    midflt_ieee_div(self, &two);

    return m;
}

static IEEE ieee_pow_neg_two(u32 m, IEEEKind kind, Rounding rounding)
{
    auto ret = midflt_ieee_one(false, kind, rounding);
    ret.exp -= m;
    return ret;
}

// n_iters      - the higher the number of iterations the higher the precision,
//                at the cost of lower performance
static IEEE ieee_approx_log2_base(const IEEE *self, int n_iters)
{
    // TODO: use an init from uint function when i make one
    auto res = midflt_ieee_init(self->exp, self->kind, self->rounding);
    auto x = midflt_ieee_mantissa(self);

    u32 m = 0;
    for (int i = 0; i < n_iters; ++i) {
        m += ieee_log2_step(&x);

        auto tmp = ieee_pow_neg_two(m, self->kind, self->rounding);
        midflt_ieee_add(&res, &tmp);
        midflt_IEEE_deinit(&tmp);

        if (midflt_ieee_is_one(&x))
            break;
    }

    midflt_IEEE_deinit(&x);
    return res;
}

// calculates the exact log2
static IEEE ieee_log2_base(const IEEE *self)
{
    // TODO: use an init from uint function when i make one
    auto res = midflt_ieee_init(self->exp, self->kind, self->rounding);
    auto x = midflt_ieee_mantissa(self);

    bool round_bit = false;
    bool sticky_bit = false;
    u32 m = 0;
    while (true) {
        m += ieee_log2_step(&x);

        if (m > (u32)res.mant.n_bits) {
            sticky_bit = true;
            break;
        } else if (m == (u32)res.mant.n_bits) {
            round_bit = true;
        } else {
            auto tmp = ieee_pow_neg_two(m, self->kind, self->rounding);
            midflt_ieee_add(&res, &tmp);
            midflt_IEEE_deinit(&tmp);
        }

        if (midflt_ieee_is_one(&x))
            break;
    }

    bool guard_bit = midint_get_bit(&res.mant, 0);
    printf("guard = %d, round = %d, sticky = %d\n", guard_bit, round_bit,
           sticky_bit);
    if (ieee_should_inc_mant(res.rounding, res.is_neg, guard_bit, round_bit,
                             sticky_bit)) {
        // allocate an extra bit for carry
        midint_ext(&res.mant, res.mant.n_bits + 1, false);
        midint_inc_bit(&res.mant, 0);

        if (midint_unsigned_sig_bits(&res.mant) == res.mant.n_bits) {
            midint_lshr_imm(&res.mant, 1);
            res.exp += 1;
        }

        midint_ext(&res.mant, res.mant.n_bits - 1, false);
    }

    midflt_IEEE_deinit(&x);
    return res;
}

void midflt_ieee_approx_log2(struct midflt_IEEE *self, int n_iters)
{
    if (self->val_cat == MIDFLT_IEEE_VAL_ZERO) {
        self->val_cat = MIDFLT_IEEE_VAL_INF;
        self->is_neg = true;
    } else if (self->val_cat == MIDFLT_IEEE_VAL_INF) {
        if (self->is_neg)
            self->val_cat = MIDFLT_IEEE_VAL_NAN;
    } else if (self->is_neg) {
        self->val_cat = MIDFLT_IEEE_VAL_NAN;
    } else if (self->val_cat == MIDFLT_IEEE_VAL_NORMAL) {
        auto tmp = ieee_approx_log2_base(self, n_iters);
        midflt_IEEE_deinit(self);
        *self = tmp;
    }
}

void midflt_ieee_log2(struct midflt_IEEE *self)
{
    if (self->val_cat == MIDFLT_IEEE_VAL_ZERO) {
        self->val_cat = MIDFLT_IEEE_VAL_INF;
        self->is_neg = true;
    } else if (self->val_cat == MIDFLT_IEEE_VAL_INF) {
        if (self->is_neg)
            self->val_cat = MIDFLT_IEEE_VAL_NAN;
    } else if (self->is_neg) {
        self->val_cat = MIDFLT_IEEE_VAL_NAN;
    } else if (self->val_cat == MIDFLT_IEEE_VAL_NORMAL) {
        auto tmp = ieee_log2_base(self);
        midflt_IEEE_deinit(self);
        *self = tmp;
    }
}

void midflt_approx_log2(struct mid_APFloat *self, int n_iters)
{
    if (midflt_kind_is_ieee(self->kind))
        midflt_ieee_approx_log2(&self->ieee, n_iters);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_log2(struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        midflt_ieee_log2(&self->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

static struct mid_APInt ieee_round_extra_mant_bits(const struct mid_APInt *mant,
                                                   IEEEKind kind,
                                                   Rounding rounding,
                                                   bool is_neg)
{
    i32 mant_size = midint_unsigned_sig_bits(mant);
    i32 norm_size = ieee_mant_n_bits(kind);
    assert(mant_size >= norm_size);

    if (mant_size == norm_size)
        return midint_copy(mant);

    i32 shift = mant_size - norm_size;

    auto ret = midint_copy(mant);

    bool guard_bit = midint_get_bit(&ret, shift);
    bool round_bit = midint_get_bit(&ret, shift - 1);
    bool sticky_bit = midint_count_trailing_zeroes(&ret) < shift - 1;

    if (ieee_should_inc_mant(rounding, is_neg, guard_bit, round_bit,
                             sticky_bit)) {
        // allocate an extra bit in case of overflow
        midint_ext(&ret, ret.n_bits + 1, false);

        midint_inc_bit(&ret, shift);
        mant_size = midint_unsigned_sig_bits(&ret);
        shift = mant_size - norm_size;
    }

    midint_lshr_imm(&ret, shift);
    midint_ext(&ret, norm_size, false);

    return ret;
}

static IEEE ieee_ln2(IEEEKind kind, Rounding rounding)
{
    // OPTIM: you could probably make this faster by precomputing the mantissa
    //        for every IEEEKind

    // mantissa of ln2 to 256 bits
    auto raw_mant_bits = midint_init_arr(256,
                                         (midint_Word[]){
                                             0x8A0D175B8BAAFA2B,
                                             0x40F343267298B62D,
                                             0xC9E3B39803F2F6AF,
                                             0xB17217F7D1CF79AB,
                                         },
                                         4, false);

    assert(midint_get_sign_bit(&raw_mant_bits));

    struct mid_APInt mant_bits =
        ieee_round_extra_mant_bits(&raw_mant_bits, kind, rounding, false);

    assert(midint_get_sign_bit(&mant_bits));

    auto ret = midflt_ieee_init_manual(&mant_bits, -1, false, kind, rounding);

    mid_APInt_deinit(&mant_bits);
    mid_APInt_deinit(&raw_mant_bits);
    return ret;
}

void midflt_ieee_approx_ln(struct midflt_IEEE *self, int n_iters)
{
    auto ln2 = ieee_ln2(self->kind, self->rounding);

    midflt_ieee_approx_log2(self, n_iters);
    midflt_ieee_mul(self, &ln2);

    midflt_IEEE_deinit(&ln2);
}

void midflt_ieee_ln(struct midflt_IEEE *self)
{
    auto ln2 = ieee_ln2(self->kind, self->rounding);

    midflt_ieee_log2(self);
    midflt_ieee_mul(self, &ln2);

    midflt_IEEE_deinit(&ln2);
}

void midflt_approx_ln(struct mid_APFloat *self, int n_iters)
{
    if (midflt_kind_is_ieee(self->kind))
        midflt_ieee_approx_ln(&self->ieee, n_iters);
    else
        MID_CRASH("unsupported APFloat kind");
}

void midflt_ln(struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        midflt_ieee_ln(&self->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_is_zero(const struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        return midflt_ieee_is_zero(&self->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_is_one(const struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        return midflt_ieee_is_one(&self->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

bool midflt_is_minus_one(const struct mid_APFloat *self)
{
    if (midflt_kind_is_ieee(self->kind))
        return midflt_ieee_is_minus_one(&self->ieee);
    else
        MID_CRASH("unsupported APFloat kind");
}

struct mid_APFloat midflt_copy(const struct mid_APFloat *src)
{
    auto res = *src;

    if (midflt_kind_is_ieee(src->kind))
        res.ieee = midflt_ieee_copy(&src->ieee);
    else
        MID_CRASH("unsupported APFloat kind");

    return res;
}
