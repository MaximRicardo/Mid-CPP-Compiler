#include "apint.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int get_active_bits(MidAPInt_Word word)
{
    // OPTIM: does the compiler optimize this to __builtin_clz?

    unsigned r = 0;

    while (word >>= 1)
        ++r;

    return r + 1;
}

// maximum nr of decimal digits to represent an unsigned int of n_bits width
static mid_isize max_dec_digits(i32 n_bits)
{
    return ceil(n_bits * log10(2.0));
}

static MidAPInt_Word uint_max_val(i32 n_bits)
{
    assert(n_bits > 0 && n_bits <= MidAPInt_word_n_bits);

    // avoids overflow if n_bits == MidAPInt_word_n_bits
    MidAPInt_Word ret = 1ULL << (n_bits - 1);
    ret += ret - 1;

    return ret;
}

static long long sint_max_val(i32 n_bits)
{
    if (n_bits == 1)
        return 0;
    else
        return uint_max_val(n_bits - 1);
}

static long long sint_min_val(i32 n_bits)
{
    if (n_bits == 1)
        return -1;
    else
        return -uint_max_val(n_bits - 1) - 1;
}

static i32 n_bits_in_last_word(i32 n_bits)
{
    return ((n_bits - 1) % MidAPInt_word_n_bits) + 1;
}

/*
static MidAPInt_Word last_word_max_val(i32 n_bits)
{
    return uint_max_val(n_bits_in_last_word(n_bits));
}
*/

static i32 get_n_words(i32 n_bits)
{
    return (n_bits + (MidAPInt_word_n_bits - 1)) / MidAPInt_word_n_bits;
}

static bool is_bignum_used(i32 n_bits)
{
    return n_bits > MidAPInt_word_n_bits;
}

static MidAPInt_Word mask_extra_bits(MidAPInt_Word word, int n_bits)
{
    if (n_bits == MidAPInt_word_n_bits) {
        return word;
    } else {
        MidAPInt_Word mask = (1ULL << n_bits) - 1;
        return word & mask;
    }
}

static MidAPInt_Word sign_ext_word(MidAPInt_Word word, int old_n_bits,
                                   int new_n_bits)
{
    MidAPInt_Word m = 1ULL << (old_n_bits - 1);
    MidAPInt_Word ret = (word ^ m) - m;
    return mask_extra_bits(ret, new_n_bits);
}

void MidAPInt_deinit(struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits))
        free(self->v.words);
}

struct Mid_APInt MidAPInt_init_arr(i32 n_bits, const MidAPInt_Word *words,
                                   i32 n_words, bool sign_ext)
{
    assert(n_words > 0);

    if (!is_bignum_used(n_bits)) {
        return MidAPInt_init_no_limit_check(n_bits, words[0], sign_ext);
    }

    bool is_signed = words[n_words - 1] & (1ULL << (MidAPInt_word_n_bits - 1));

    i32 dest_n_words = get_n_words(n_bits);

    struct Mid_APInt ret = {.n_bits = n_bits};
    ret.v.words = Mid_malloc(dest_n_words * sizeof(*ret.v.words));

    for (i32 i = 0; i < dest_n_words; ++i) {
        if (i < n_words)
            ret.v.words[i] = words[i];
        else
            ret.v.words[i] = (sign_ext && is_signed) ? MidAPInt_word_max : 0;
    }

    auto last = &ret.v.words[dest_n_words - 1];
    *last = mask_extra_bits(*last, n_bits_in_last_word(ret.n_bits));

    return ret;
}

static struct Mid_APInt apint_init_impl(i32 n_bits, MidAPInt_Word val,
                                        bool is_signed, bool limit_check)
{
    assert(n_bits > 0);

    if (is_bignum_used(n_bits)) {
        return MidAPInt_init_arr(n_bits, &val, 1, is_signed);
    }

    if (limit_check) {
        if (is_signed) {
            long long s_val = val;
            if (s_val > sint_max_val(n_bits) || s_val < sint_min_val(n_bits))
                MID_CRASH("signed val doesn't fit in n_bits");
        } else {
            if (val > uint_max_val(n_bits))
                MID_CRASH("unsigned val doesn't fit in n_bits");
        }
    }

    return (struct Mid_APInt){.n_bits = n_bits,
                              .v.val = mask_extra_bits(val, n_bits)};
}

struct Mid_APInt MidAPInt_init(i32 n_bits, MidAPInt_Word val, bool is_signed)
{
    return apint_init_impl(n_bits, val, is_signed, true);
}

struct Mid_APInt MidAPInt_init_no_limit_check(i32 n_bits, MidAPInt_Word val,
                                              bool is_signed)
{
    return apint_init_impl(n_bits, val, is_signed, false);
}

struct Mid_APInt MidAPInt_zero(i32 n_bits)
{
    struct Mid_APInt ret = {.n_bits = n_bits};

    if (!is_bignum_used(n_bits))
        ret.v.val = 0;
    else
        ret.v.words = Mid_calloc(get_n_words(n_bits), sizeof(*ret.v.words));

    return ret;
}

struct Mid_APInt MidAPInt_copy(const struct Mid_APInt *src)
{
    struct Mid_APInt dest = {.n_bits = src->n_bits};

    if (is_bignum_used(src->n_bits)) {
        size_t size = get_n_words(src->n_bits) * sizeof(*src->v.words);
        dest.v.words = Mid_malloc(size);
        memcpy(dest.v.words, src->v.words, size);
    } else {
        dest.v.val = src->v.val;
    }

    return dest;
}

void MidAPInt_copy_value(struct Mid_APInt *restrict dest,
                         const struct Mid_APInt *restrict src)
{
    assert(dest->n_bits == src->n_bits);

    if (is_bignum_used(dest->n_bits)) {
        memcpy(dest->v.words, src->v.words,
               get_n_words(dest->n_bits) * sizeof(*dest->v.words));
    } else {
        dest->v.val = src->v.val;
    }
}

bool MidAPInt_is_zero(const struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = 0; i < get_n_words(self->n_bits); ++i) {
            if (self->v.words[i] != 0)
                return false;
        }

        return true;
    } else {
        return self->v.val == 0;
    }
}

static bool is_word_signed_min(MidAPInt_Word word, int n_bits)
{
    if (word == 0)
        return false;

    // mask away the sign bit
    // if the result is zero then that was the only active bit and the value
    // is the signed minimum
    return (word & ~(1ULL << (n_bits - 1))) == 0;
}

bool MidAPInt_is_signed_min(const struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);
        for (i32 i = 0; i < n_words - 1; ++i) {
            if (self->v.words[i] != 0)
                return false;
        }

        return is_word_signed_min(self->v.words[n_words - 1],
                                  n_bits_in_last_word(self->n_bits));
    } else {
        return is_word_signed_min(self->v.val, self->n_bits);
    }
}

static bool word_is_all_ones(MidAPInt_Word word, int n_bits)
{
    if (n_bits == MidAPInt_word_n_bits) {
        return word == MidAPInt_word_max;
    } else {
        // set all unused bits high
        MidAPInt_Word mask = ~((1ULL << n_bits) - 1);
        word = word | mask;

        return word == MidAPInt_word_max;
    }
}

bool MidAPInt_is_all_ones(const struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);
        if (!word_is_all_ones(self->v.words[n_words - 1],
                              n_bits_in_last_word(self->n_bits)))
            return false;

        for (i32 i = 0; i < n_words - 1; ++i) {
            if (self->v.words[i] != MidAPInt_word_max)
                return false;
        }

        return true;
    } else {
        return word_is_all_ones(self->v.val, self->n_bits);
    }
}

void MidAPInt_ext(struct Mid_APInt *self, i32 new_n_bits, bool sign_ext)
{
    bool old_uses_bignum = is_bignum_used(self->n_bits);
    bool new_uses_bignum = is_bignum_used(new_n_bits);

    if (old_uses_bignum && new_uses_bignum) {
        i32 n_words = get_n_words(self->n_bits);
        i32 n_new_words = get_n_words(new_n_bits);
        self->v.words =
            Mid_realloc(self->v.words, n_new_words * sizeof(*self->v.words));

        if (new_n_bits < self->n_bits) {
            auto last = &self->v.words[n_new_words - 1];
            *last = mask_extra_bits(*last, new_n_bits);
        } else if (n_new_words > n_words) {
            for (auto i = n_words; i < n_new_words; ++i) {
                self->v.words[i] = sign_ext ? MidAPInt_word_max : 0;
            }
        }
    } else if (old_uses_bignum) {
        auto lsw = self->v.words[0];
        free(self->v.words);
        self->v.val = mask_extra_bits(lsw, new_n_bits);
    } else if (new_uses_bignum) {
        auto lsw = self->v.val;
        if (sign_ext)
            lsw = sign_ext_word(lsw, self->n_bits, MidAPInt_word_n_bits);
        *self = MidAPInt_init(new_n_bits, lsw, sign_ext);
    } else {
        if (new_n_bits > self->n_bits && sign_ext)
            self->v.val = sign_ext_word(self->v.val, self->n_bits, new_n_bits);
        else
            self->v.val = mask_extra_bits(self->v.val, new_n_bits);
    }

    self->n_bits = new_n_bits;
}

bool MidAPInt_get_bit(const struct Mid_APInt *self, i32 n)
{
    assert(n > 0 && n < self->n_bits);

    if (is_bignum_used(self->n_bits)) {
        i32 word_idx = get_n_words(n + 1) - 1;
        const MidAPInt_Word *word = &self->v.words[word_idx];

        i32 n_in_word = n - word_idx * MidAPInt_word_n_bits;
        return (*word >> n_in_word) & 1;
    } else {
        return (self->v.val >> n) & 1;
    }
}

bool MidAPInt_get_sign_bit(const struct Mid_APInt *self)
{
    return MidAPInt_get_bit(self, self->n_bits - 1);
}

static void log_uint_bignum(const struct Mid_APInt *self, FILE *out)
{
    auto tmp = MidAPInt_copy(self);
    auto ten = MidAPInt_init(tmp.n_bits, 10, false);

    char *digits = Mid_malloc(max_dec_digits(self->n_bits) * sizeof(*digits));

    mid_isize i = 0;
    while (MidAPInt_is_ugteq(&tmp, &ten)) {
        struct Mid_APInt d;
        MidAPInt_udivrem(&tmp, &ten, &tmp, &d);

        // d is guaranteed to be less than 10 so we can just take it from
        // the first word
        digits[i] = d.v.words[0] + '0';

        MidAPInt_deinit(&d);
        ++i;
    }

    auto d = MidAPInt_nip_urem(&tmp, &ten);
    digits[i] = d.v.words[0] + '0';

    for (; i >= 0; --i) {
        fputc(digits[i], out);
    }

    free(digits);
    MidAPInt_deinit(&d);
    MidAPInt_deinit(&ten);
    MidAPInt_deinit(&tmp);
}

static void log_uint(const struct Mid_APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        log_uint_bignum(self, out);
    } else {
        fprintf(out, "%" MIDAPINT_WORD_UNSIGNED_FORMAT, self->v.val);
    }
}

static void log_sint(const struct Mid_APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        bool is_negative = MidAPInt_get_sign_bit(self);
        if (!is_negative) {
            log_uint_bignum(self, out);
        } else if (MidAPInt_is_signed_min(self)) {
            // we can't negate self without overflowing so we need to
            // allocate an extra bit
            assert(self->n_bits < INT32_MAX);

            fputc('-', out);

            auto tmp = MidAPInt_copy(self);
            MidAPInt_ext(&tmp, self->n_bits + 1, true);

            MidAPInt_negate(&tmp);
            log_uint_bignum(&tmp, out);

            MidAPInt_deinit(&tmp);
        } else {
            // negate the number and print its unsigned version
            fputc('-', out);

            auto tmp = MidAPInt_copy(self);
            MidAPInt_negate(&tmp);
            log_uint_bignum(&tmp, out);
            MidAPInt_deinit(&tmp);
        }
    } else {
        fprintf(out, "%" MIDAPINT_WORD_SIGNED_FORMAT,
                sign_ext_word(self->v.val, self->n_bits, MidAPInt_word_n_bits));
    }
}

void MidAPInt_log(const struct Mid_APInt *self, FILE *out, bool is_signed)
{
    if (is_signed)
        log_sint(self, out);
    else
        log_uint(self, out);
}

void MidAPInt_log_hex(const struct Mid_APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        bool printed = false;
        for (i32 i = get_n_words(self->n_bits) - 1; i >= 1; --i) {
            if (self->v.words[i] == 0)
                continue;
            printed = true;
            fprintf(out, "%" MIDAPINT_WORD_HEX_FORMAT, self->v.words[i]);
        }

        if (printed)
            fprintf(out, "%" MIDAPINT_WORD_FULL_HEX_FORMAT, self->v.words[0]);
        else
            fprintf(out, "%" MIDAPINT_WORD_HEX_FORMAT, self->v.words[0]);
    } else {
        fprintf(out, "%" MIDAPINT_WORD_HEX_FORMAT, self->v.val);
    }
}

i32 MidAPInt_n_active_bits(const struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = get_n_words(self->n_bits) - 1; i >= 0; --i) {
            auto word = self->v.words[i];
            if (word != 0)
                return get_active_bits(word) + i * MidAPInt_word_n_bits;
        }

        return 0;
    } else {
        return get_active_bits(self->v.val);
    }
}

void MidAPInt_mask_extra_bits(struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 last = get_n_words(self->n_bits) - 1;
        self->v.words[last] = mask_extra_bits(
            self->v.words[last], n_bits_in_last_word(self->n_bits));
    } else {
        self->v.val = mask_extra_bits(self->v.val, self->n_bits);
    }
}

void MidAPInt_add(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        bool carry = false;
        for (i32 i = 0; i < get_n_words(a->n_bits); ++i) {
            auto old = a->v.words[i];

            a->v.words[i] += b->v.words[i];

            // the word might have wrapped around or might be about to wrap
            // around due to the previous carry
            bool new_carry = a->v.words[i] < old ||
                             (carry && a->v.words[i] == MidAPInt_word_max);

            a->v.words[i] += carry;

            carry = new_carry;
        }
    } else {
        a->v.val += b->v.val;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }

    MidAPInt_mask_extra_bits(a);
}

void MidAPInt_add_imm(struct Mid_APInt *a, u64 b)
{
    if (is_bignum_used(a->n_bits)) {
        a->v.words[0] += b;
        bool carry = a->v.words[0] < b;

        for (i32 i = 1; i < get_n_words(a->n_bits); ++i) {
            a->v.words[i] += carry;
            carry = carry && a->v.words[i] == MidAPInt_word_max;
        }
    } else {
        a->v.val += b;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }

    MidAPInt_mask_extra_bits(a);
}

void MidAPInt_sub(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        bool borrow = false;
        for (i32 i = 0; i < get_n_words(a->n_bits); ++i) {
            auto old = a->v.words[i];

            a->v.words[i] -= b->v.words[i];

            // the word might have wrapped around or might be about to wrap
            // around due to the previous carry
            bool new_borrow =
                a->v.words[i] > old || (borrow && a->v.words[i] == 0);

            a->v.words[i] -= borrow;

            borrow = new_borrow;
        }
    } else {
        a->v.val -= b->v.val;
    }

    MidAPInt_mask_extra_bits(a);
}

void MidAPInt_sub_imm(struct Mid_APInt *a, u64 b)
{
    if (is_bignum_used(a->n_bits)) {
        auto old = a->v.words[0];
        a->v.words[0] -= b;
        bool borrow = old < a->v.words[0];

        for (i32 i = 0; i < get_n_words(a->n_bits); ++i) {
            a->v.words[i] -= borrow;
            borrow = borrow && a->v.words[i] == 0;
        }
    } else {
        a->v.val -= b;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }

    MidAPInt_mask_extra_bits(a);
}

static MidAPInt_Word get_low_half(MidAPInt_Word word)
{
    MidAPInt_Word mask = (1ULL << (MidAPInt_word_n_bits / 2)) - 1;
    return word & mask;
}

static MidAPInt_Word get_high_half(MidAPInt_Word word)
{
    return word >> (MidAPInt_word_n_bits / 2);
}

// lowkirkenuinelly no idea how this works, look at LLVM's APInt source code
// for documentation
static int tc_multiply_part(MidAPInt_Word *dst, MidAPInt_Word *src,
                            MidAPInt_Word mul, MidAPInt_Word carry,
                            i32 src_parts, i32 dst_parts, bool add)
{
    assert(dst <= src || dst >= src + src_parts);
    assert(0 <= dst_parts);
    assert(dst_parts <= src_parts);

    i32 n = MID_MIN(dst_parts, src_parts);

    for (i32 i = 0; i < n; ++i) {
        MidAPInt_Word src_part = src[i];
        MidAPInt_Word low, mid, high;
        if (mul == 0 || src_part == 0) {
            low = carry;
            high = 0;
        } else {
            low = get_low_half(src_part) * get_low_half(mul);
            high = get_high_half(src_part) * get_high_half(mul);

            mid = get_low_half(src_part) * get_high_half(mul);
            high += get_high_half(mid);
            mid <<= MidAPInt_word_n_bits / 2;
            if (low + mid < low)
                ++high;
            low += mid;

            mid = get_high_half(src_part) * get_low_half(mul);
            high += get_high_half(mid);
            mid <<= MidAPInt_word_n_bits / 2;
            if (low + mid < low)
                ++high;
            low += mid;

            // add carry
            if (low + carry < low)
                ++high;
            low += carry;
        }

        if (add) {
            if (low + dst[i] < low)
                ++high;
            dst[i] += low;
        } else {
            dst[i] = low;
        }

        carry = high;
    }

    if (src_parts < dst_parts) {
        // full multiplication, there is no overflow
        assert(src_parts + 1 == dst_parts);
        dst[src_parts] = carry;
        return 0;
    }

    // we overflowed if there is a carry
    if (carry)
        return 1;

    // we would overflow if any significant unwritten parts would be non-zero.
    // this is true if any remaining src parts are non-zero and the
    // multiplier is non-zero.
    if (mul) {
        for (i32 i = dst_parts; i < src_parts; ++i) {
            if (src[i])
                return 1;
        }
    }

    // we fit into the narrow destination
    return 0;
}

// dst = lhs * rhs, where dst has the same width as the operands.
// returns one if overflow occurred, otherwise zero.
// dst can not alias with lhs or rhs
static int tc_multiply(MidAPInt_Word *restrict dst, MidAPInt_Word *lhs,
                       MidAPInt_Word *rhs, i32 parts)
{
    int overflow = 0;

    for (i32 i = 0; i < parts; ++i) {
        overflow |=
            tc_multiply_part(&dst[i], lhs, rhs[i], 0, parts, parts - i, i != 0);
    }

    return overflow;
}

struct Mid_APInt MidAPInt_nip_mul(const struct Mid_APInt *a,
                                  const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (!is_bignum_used(a->n_bits))
        return MidAPInt_init_no_limit_check(a->n_bits, a->v.val * b->v.val,
                                            false);

    auto res = MidAPInt_zero(a->n_bits);
    tc_multiply(res.v.words, a->v.words, b->v.words, get_n_words(a->n_bits));
    MidAPInt_mask_extra_bits(&res);
    return res;
}

struct Mid_APInt MidAPInt_nip_mul_imm(const struct Mid_APInt *a, u64 b)
{
    if (!is_bignum_used(a->n_bits)) {
        return MidAPInt_init_no_limit_check(a->n_bits, a->v.val * b, false);
    } else {
        auto n_words = get_n_words(a->n_bits);
        auto res = MidAPInt_zero(a->n_bits);
        tc_multiply_part(res.v.words, a->v.words, b, 0, n_words, n_words,
                         false);
        return res;
    }
}

void MidAPInt_mul_imm(struct Mid_APInt *a, u64 b)
{
    if (!is_bignum_used(a->n_bits)) {
        a->v.val *= b;
    } else {
        auto n_words = get_n_words(a->n_bits);
        tc_multiply_part(a->v.words, a->v.words, b, 0, n_words, n_words, false);
    }

    MidAPInt_mask_extra_bits(a);
}

void MidAPInt_mul(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    auto tmp = MidAPInt_nip_mul(a, b);
    MidAPInt_deinit(a);
    *a = tmp;
}

static void shl_bignum_case(struct Mid_APInt *a, i32 count)
{
    i32 word_shift = count / MidAPInt_word_n_bits;
    i32 bit_shift = count % MidAPInt_word_n_bits;

    i32 n_words = get_n_words(a->n_bits);

    if (bit_shift == 0) {
        // shift whole words at a time if we don't need to shift individual
        // bits within them
        memmove(&a->v.words[word_shift], a->v.words,
                (n_words - word_shift) * sizeof(*a->v.words));
    } else {
        i32 i = n_words;
        while (i-- > word_shift) {
            a->v.words[i] = a->v.words[i - word_shift] << bit_shift;
            // account for the preceding word's bits
            if (i > word_shift)
                a->v.words[i] |= a->v.words[i - word_shift - 1] >>
                                 (MidAPInt_word_n_bits - bit_shift);
        }
    }

    // fill the remainder with 0s
    memset(a->v.words, 0, word_shift * sizeof(*a->v.words));

    MidAPInt_mask_extra_bits(a);
}

static void lshr_bignum_case(struct Mid_APInt *a, i32 count)
{
    i32 word_shift = count / MidAPInt_word_n_bits;
    i32 bit_shift = count % MidAPInt_word_n_bits;

    i32 n_words = get_n_words(a->n_bits);

    if (bit_shift == 0) {
        // shift whole words at a time if we don't need to shift individual
        // bits within them
        memmove(a->v.words, &a->v.words[word_shift],
                (n_words - word_shift) * sizeof(*a->v.words));
    } else {
        i32 i = -1;
        i32 cap = n_words - word_shift - 1;
        while (i++ < cap) {
            a->v.words[i] = a->v.words[i + word_shift] >> bit_shift;
            // account for the preceding word's bits
            if (i < cap)
                a->v.words[i] |= a->v.words[i + word_shift + 1]
                                 << (MidAPInt_word_n_bits - bit_shift);
        }
    }

    // fill the remainder with 0s
    memset(&a->v.words[n_words - word_shift], 0,
           word_shift * sizeof(*a->v.words));

    MidAPInt_mask_extra_bits(a);
}

// guarantees arithmetic right shift even on systems where right shifting
// a signed integer is still logical
static u64 shift_arith_right(u64 val, unsigned sh)
{
    u64 mask = (1ULL << 63);
    u64 result = (val >> sh) | -((val & mask) >> sh);
    return result;
}

static void ashr_bignum_case(struct Mid_APInt *a, i32 count)
{
    i32 word_shift = count / MidAPInt_word_n_bits;
    i32 bit_shift = count % MidAPInt_word_n_bits;

    i32 n_words = get_n_words(a->n_bits);
    i32 words_to_move = n_words - word_shift;

    bool is_neg = MidAPInt_get_sign_bit(a);

    if (words_to_move != 0) {
        // fill in the last word via sign extension
        a->v.words[n_words - 1] =
            sign_ext_word(a->v.words[n_words - 1],
                          n_bits_in_last_word(a->n_bits), MidAPInt_word_n_bits);

        if (bit_shift == 0) {
            // shortcut for shifting whole words
            memmove(a->v.words, &a->v.words[word_shift],
                    words_to_move * sizeof(*a->v.words));
        } else {
            // move the words containing significant bits
            for (i32 i = 0; i < words_to_move - 1; ++i) {
                a->v.words[i] = (a->v.words[i + word_shift] >> bit_shift) |
                                (a->v.words[i + word_shift + 1]
                                 << (MidAPInt_word_n_bits - bit_shift));
            }

            // the last word needs to be arithmetic right shifted
            a->v.words[words_to_move - 1] =
                shift_arith_right(a->v.words[words_to_move - 1], bit_shift);
        }
    }

    // fill the remainder with the sign bit
    memset(&a->v.words[n_words - word_shift], is_neg ? -1 : 0,
           word_shift * sizeof(*a->v.words));

    MidAPInt_mask_extra_bits(a);
}

void MidAPInt_shl(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    auto b_bits = MidAPInt_n_active_bits(b);
    if (b_bits >= log2(a->n_bits))
        MID_CRASH("shift amount too high");

    // the maximum nr of bits is INT32_MAX so the maximum shift amount should
    // be able to fit in a single word
    assert(!is_bignum_used(b_bits));
    MidAPInt_shl_imm(a, b->v.words[0]);
}

void MidAPInt_shl_imm(struct Mid_APInt *a, u64 count)
{
    assert(count < (u64)a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        shl_bignum_case(a, count);
    } else {
        a->v.val <<= count;
        MidAPInt_mask_extra_bits(a);
    }
}

void MidAPInt_lshr(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    auto b_bits = MidAPInt_n_active_bits(b);
    if (b_bits >= log2(a->n_bits))
        MID_CRASH("shift amount too high");

    // the maximum nr of bits is INT32_MAX so the maximum shift amount should
    // be able to fit in a single word
    assert(!is_bignum_used(b_bits));
    MidAPInt_lshr_imm(a, b->v.words[0]);
}

void MidAPInt_lshr_imm(struct Mid_APInt *a, u64 count)
{
    assert(count < (u64)a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        lshr_bignum_case(a, count);
    } else {
        a->v.val >>= count;
        MidAPInt_mask_extra_bits(a);
    }
}

void MidAPInt_ashr(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    auto b_bits = MidAPInt_n_active_bits(b);
    if (b_bits >= log2(a->n_bits))
        MID_CRASH("shift amount too high");

    // the maximum nr of bits is INT32_MAX so the maximum shift amount should
    // be able to fit in a single word
    assert(!is_bignum_used(b_bits));
    MidAPInt_ashr_imm(a, b->v.words[0]);
}

void MidAPInt_ashr_imm(struct Mid_APInt *a, u64 count)
{
    assert(count < (u64)a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        ashr_bignum_case(a, count);
    } else {
        auto full_word =
            sign_ext_word(a->v.val, a->n_bits, MidAPInt_word_n_bits);
        a->v.val = shift_arith_right(full_word, count);
        MidAPInt_mask_extra_bits(a);
    }
}

struct Mid_APInt MidAPInt_nip_add(const struct Mid_APInt *a,
                                  const struct Mid_APInt *b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_add(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_add_imm(const struct Mid_APInt *a, u64 b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_add_imm(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_sub(const struct Mid_APInt *a,
                                  const struct Mid_APInt *b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_sub(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_sub_imm(const struct Mid_APInt *a, u64 b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_sub_imm(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_shl(const struct Mid_APInt *a,
                                  const struct Mid_APInt *b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_shl(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_shl_imm(const struct Mid_APInt *a, u64 b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_shl_imm(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_lshr(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_lshr(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_lshr_imm(const struct Mid_APInt *a, u64 b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_lshr_imm(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_ashr(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_ashr(&res, b);
    return res;
}

struct Mid_APInt MidAPInt_nip_ashr_imm(const struct Mid_APInt *a, u64 b)
{
    struct Mid_APInt res = MidAPInt_copy(a);
    MidAPInt_ashr_imm(&res, b);
    return res;
}

static int countl_zero_32(u32 num)
{
    if (num == 0)
        return 32;

    int zeroes = 0;
    for (u32 shift = 16; shift; shift >>= 1) {
        auto tmp = num >> shift;
        if (tmp)
            num = tmp;
        else
            zeroes |= shift;
    }

    return zeroes;
}

/*
 * u is the dividend (lhs) and is m + n digits long
 * v is the divisor (rhs) and is n digits long
 * q = u / v
 * r (optional) = u % v;
 */
static void knuth_bignum_div(u32 *u, u32 *v, u32 *q, u32 *r, u32 m, u32 n)
{
    // i have no clue how any of this works ngl i just copied it from LLVM

    assert(n > 1);

    u64 b = 1ULL << 32;

    unsigned shift = countl_zero_32(v[n - 1]);
    u32 v_carry = 0;
    u32 u_carry = 0;
    if (shift) {
        for (u32 i = 0; i < m + n; ++i) {
            u32 u_tmp = u[i] >> (32 - shift);
            u[i] = (u[i] << shift) | u_carry;
            u_carry = u_tmp;
        }
        for (u32 i = 0; i < n; ++i) {
            u32 v_tmp = v[i] >> (32 - shift);
            v[i] = (v[i] << shift) | v_carry;
            v_carry = v_tmp;
        }
    }
    u[m + n] = u_carry;

    i32 j = m;
    do {
        u64 low_32_mask = 0xffffffff;
        u64 dividend = ((u64)u[j + n] << 32) | u[j + n - 1];
        u64 qp = dividend / v[n - 1];
        u64 rp = dividend % v[n - 1];
        if (qp == b || qp * v[n - 2] > b * rp + u[j + n - 2]) {
            --qp;
            rp += v[n - 1];
            if (rp < b && (qp == b || qp * v[n - 2] > b * rp + u[j + n - 2]))
                --qp;
        }

        i64 borrow = 0;
        for (u32 i = 0; i < n; ++i) {
            u64 p = qp * v[i];
            i64 subres = u[j + i] - borrow - (p & low_32_mask);
            u[j + i] = subres;
            borrow = (p >> 32) - ((u64)subres >> 32);
        }
        bool is_neg = u[j + n] < borrow;
        u[j + n] -= borrow & low_32_mask;

        q[j] = qp;
        if (is_neg) {
            --q[j];

            bool carry = false;
            for (u32 i = 0; i < n; ++i) {
                u32 limit = MID_MIN(u[j + i], v[i]);
                u[j + i] += v[i] + carry;
                carry = u[j + i] < limit || (carry && u[j + i] == limit);
            }
            u[j + n] += carry;
        }
    } while (--j >= 0);

    if (r) {
        if (shift) {
            u32 carry = 0;
            for (i32 i = n - 1; i >= 0; --i) {
                r[i] = (u[i] >> shift) | carry;
                carry = u[i] << (32 - shift);
            }
        } else {
            for (i32 i = n - 1; i >= 0; --i) {
                r[i] = u[i];
            }
        }
    }
}

static void convert_to_u32_digits(const MidAPInt_Word *words, i32 n_words,
                                  u32 *digits)
{
    for (i32 i = 0; i < n_words; ++i) {
        digits[i * 2] = words[i];
        digits[i * 2 + 1] = words[i] >> 32;
    }
}

// i would like to say i understand any of this but i really dont
static void bignum_div(const MidAPInt_Word *a, i32 a_n_words,
                       const MidAPInt_Word *b, i32 b_n_words,
                       MidAPInt_Word *out_quot, MidAPInt_Word *out_rem)
{
    assert(a_n_words >= b_n_words);

    // assumes MidAPInt_Word is 64 bits
    static_assert(sizeof(MidAPInt_Word) == sizeof(u64));

    i32 n = b_n_words * 2;
    i32 m = a_n_words * 2 - n;

    u32 *u = Mid_malloc((m + n + 1) * sizeof(*u));
    u32 *v = Mid_malloc(n * sizeof(*v));
    u32 *q = Mid_malloc((m + n) * sizeof(*q));
    u32 *r = out_rem ? Mid_malloc(n * sizeof(*r)) : NULL;

    convert_to_u32_digits(a, a_n_words, u);
    u[m + n] = 0; // account for spill in the knuth algorithm

    convert_to_u32_digits(b, b_n_words, v);

    memset(q, 0, (m + n) * sizeof(*q));
    if (r)
        memset(r, 0, n * sizeof(*r));

    // adjust m and n for knuth division: n is the number of words in the
    // divisor, and m is the number of words by which the dividend exceeds the
    // divisor (i.e. the length of the dividend is m + n)
    for (i32 i = n; i > 0 && v[i - 1] == 0; --i) {
        --n;
        ++m;
    }
    for (i32 i = m + n; i > 0 && u[i - 1] == 0; --i)
        --m;

    if (n == 0)
        MID_CRASH("division by 0");

    // knuth division requires the divisor be at least 2 digits long, so in
    // the case the divisor is a single word we do the short division
    // algorithm
    // btw i have no idea how this works either i just copied this from LLVM
    if (n == 1) {
        u32 divisor = v[0];
        u32 rem = 0;
        u64 low_32_mask = 0xffffffff;
        for (i32 i = m; i >= 0; --i) {
            u64 partial_dividend = ((u64)rem << 32) | u[i];
            if (partial_dividend == 0) {
                q[i] = 0;
                rem = 0;
            } else if (partial_dividend < divisor) {
                q[i] = 0;
                rem = partial_dividend & low_32_mask;
            } else if (partial_dividend == divisor) {
                q[i] = 1;
                rem = 0;
            } else {
                q[i] = (partial_dividend / divisor) & low_32_mask;
                rem = (partial_dividend - q[i] * divisor) & low_32_mask;
            }
        }
        if (r)
            r[0] = rem;
    } else {
        knuth_bignum_div(u, v, q, r, m, n);
    }

    if (out_quot) {
        for (i32 i = 0; i < a_n_words; ++i) {
            out_quot[i] = q[i * 2];
            out_quot[i] |= (u64)q[i * 2 + 1] << 32;
        }
    }

    if (out_rem) {
        for (i32 i = 0; i < b_n_words; ++i) {
            out_rem[i] = r[i * 2];
            out_rem[i] |= (u64)r[i * 2 + 1] << 32;
        }
    }

    free(u);
    free(v);
    free(q);
    free(r);
}

struct Mid_APInt MidAPInt_nip_udiv(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (!is_bignum_used(a->n_bits)) {
        if (b->v.val == 0)
            MID_CRASH("division by zero");
        return MidAPInt_init(a->n_bits, a->v.val / b->v.val, false);
    } else {
        i32 a_words = get_n_words(MidAPInt_n_active_bits(a));
        i32 b_bits = MidAPInt_n_active_bits(b);
        i32 b_words = get_n_words(b_bits);

        // degenerate cases
        if (b_bits == 0)
            MID_CRASH("division by zero");
        if (a_words == 0)
            // 0 / x = 0
            return MidAPInt_zero(a->n_bits);
        if (b_bits == 1)
            // x / 1 = x
            return MidAPInt_copy(a);
        if (a_words < b_words || MidAPInt_is_ult(a, b))
            // x / y = 0 if x < y
            return MidAPInt_zero(a->n_bits);
        if (MidAPInt_is_eq(a, b))
            // x / x = 1
            return MidAPInt_init(a->n_bits, 1, false);
        if (a_words == 1) // b_words must also be 1 in this case cuz it can't
                          // be less
            return MidAPInt_init(a->n_bits, a->v.words[0] / b->v.words[0],
                                 false);

        struct Mid_APInt quot = MidAPInt_zero(a->n_bits);
        bignum_div(a->v.words, a_words, b->v.words, b_words, quot.v.words,
                   NULL);
        return quot;
    }
}

struct Mid_APInt MidAPInt_nip_sdiv(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b)
{
    bool a_neg = MidAPInt_get_sign_bit(a);
    bool b_neg = MidAPInt_get_sign_bit(b);

    if (a_neg && b_neg) {
        // -a / -b = +c
        auto pos_a = MidAPInt_nip_negate(a);
        auto pos_b = MidAPInt_nip_negate(b);
        auto res = MidAPInt_nip_udiv(&pos_a, &pos_b);

        MidAPInt_deinit(&pos_a);
        MidAPInt_deinit(&pos_b);
        return res;
    } else if (a_neg) {
        // -a / +b = -c
        auto pos_a = MidAPInt_nip_negate(a);
        auto res = MidAPInt_nip_udiv(&pos_a, b);
        MidAPInt_negate(&res);

        MidAPInt_deinit(&pos_a);
        return res;
    } else if (b_neg) {
        // +a / -b = -c
        auto pos_b = MidAPInt_nip_negate(b);
        auto res = MidAPInt_nip_udiv(a, &pos_b);
        MidAPInt_negate(&res);

        MidAPInt_deinit(&pos_b);
        return res;
    } else {
        // +a / +b = +c
        return MidAPInt_nip_udiv(a, b);
    }
}

void MidAPInt_udiv(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    auto tmp = MidAPInt_nip_udiv(a, b);
    MidAPInt_deinit(a);
    *a = tmp;
}

void MidAPInt_sdiv(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    auto tmp = MidAPInt_nip_sdiv(a, b);
    MidAPInt_deinit(a);
    *a = tmp;
}

void MidAPInt_urem(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    auto tmp = MidAPInt_nip_urem(a, b);
    MidAPInt_deinit(a);
    *a = tmp;
}

void MidAPInt_srem(struct Mid_APInt *a, const struct Mid_APInt *b)
{
    auto tmp = MidAPInt_nip_srem(a, b);
    MidAPInt_deinit(a);
    *a = tmp;
}

struct Mid_APInt MidAPInt_nip_urem(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (!is_bignum_used(a->n_bits)) {
        if (b->v.val == 0)
            MID_CRASH("remainder by zero");
        return MidAPInt_init(a->n_bits, a->v.val % b->v.val, false);
    } else {
        i32 a_words = get_n_words(MidAPInt_n_active_bits(a));
        i32 b_bits = MidAPInt_n_active_bits(b);
        i32 b_words = get_n_words(b_bits);

        // degenerate cases
        if (b_bits == 0)
            MID_CRASH("remainder by zero");
        if (a_words == 0)
            // 0 % x = 0
            return MidAPInt_zero(a->n_bits);
        if (b_bits == 1)
            // x % 1 = 0
            return MidAPInt_zero(a->n_bits);
        if (a_words < b_words || MidAPInt_is_ult(a, b))
            // x % y = x if x < y
            return MidAPInt_copy(a);
        if (MidAPInt_is_eq(a, b))
            // x % x = 0
            return MidAPInt_zero(a->n_bits);
        if (a_words == 1) // b_words must also be 1 in this case cuz it can't
                          // be greater than a_words
            return MidAPInt_init(a->n_bits, a->v.words[0] % b->v.words[0],
                                 false);
        if (MidAPInt_is_pow2(b)) {
            // x % 2^w == x & (2^w - 1)
            // TODO: implement this optimization

            /*
            struct Mid_APInt rem = MidAPInt_copy(a);
            MidAPInt_clear_bits(&rem, );
            */
        }

        struct Mid_APInt rem = MidAPInt_zero(a->n_bits);
        bignum_div(a->v.words, a_words, b->v.words, b_words, NULL, rem.v.words);
        return rem;
    }
}

struct Mid_APInt MidAPInt_nip_srem(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b)
{
    bool a_neg = MidAPInt_get_sign_bit(a);
    bool b_neg = MidAPInt_get_sign_bit(b);

    if (a_neg && b_neg) {
        // -a % -b = -c
        auto pos_a = MidAPInt_nip_negate(a);
        auto pos_b = MidAPInt_nip_negate(b);
        auto res = MidAPInt_nip_urem(&pos_a, &pos_b);
        MidAPInt_negate(&res);

        MidAPInt_deinit(&pos_a);
        MidAPInt_deinit(&pos_b);
        return res;
    } else if (a_neg) {
        // -a % +b = -c
        auto pos_a = MidAPInt_nip_negate(a);
        auto res = MidAPInt_nip_urem(&pos_a, b);
        MidAPInt_negate(&res);

        MidAPInt_deinit(&pos_a);
        return res;
    } else if (b_neg) {
        // +a % -b = +c
        auto pos_b = MidAPInt_nip_negate(b);
        auto res = MidAPInt_nip_urem(a, &pos_b);

        MidAPInt_deinit(&pos_b);
        return res;
    } else {
        // +a % +b = +c
        return MidAPInt_nip_urem(a, b);
    }
}

void MidAPInt_udivrem(const struct Mid_APInt *a, const struct Mid_APInt *b,
                      struct Mid_APInt *out_quot, struct Mid_APInt *out_rem)
{
    assert(out_quot && out_rem);
    assert(a->n_bits == b->n_bits);

    struct Mid_APInt quot;
    struct Mid_APInt rem;

    if (!is_bignum_used(a->n_bits)) {
        if (a->v.val == 0)
            MID_CRASH("division by 0");

        quot = MidAPInt_init(a->n_bits, a->v.val / b->v.val, false);
        rem = MidAPInt_init(a->n_bits, a->v.val % b->v.val, false);
        goto finish_normal;
    }

    i32 a_words = get_n_words(MidAPInt_n_active_bits(a));
    i32 b_bits = MidAPInt_n_active_bits(b);
    i32 b_words = get_n_words(b_bits);

    // degenerate cases
    if (a_words == 0) {
        quot = MidAPInt_zero(a->n_bits); // 0 / x = 0
        rem = MidAPInt_zero(a->n_bits);  // 0 % x = 0
        goto finish_normal;
    }
    if (b_bits == 1) {
        rem = MidAPInt_zero(a->n_bits); // x % 1 = 0
        goto finish_cpy_a_to_quot;      // x / 1 = x
    }
    if (a_words < b_words || MidAPInt_is_ult(a, b)) {
        quot = MidAPInt_zero(a->n_bits); // x / y = 0 if x < y
        goto finish_cpy_a_to_rem;        // x % y = x if x < y
    }
    if (MidAPInt_is_eq(a, b)) {
        quot = MidAPInt_init(a->n_bits, 1, false); // x / x = 1
        rem = MidAPInt_zero(a->n_bits);            // x % x = 0
        goto finish_normal;
    }

    quot = MidAPInt_zero(a->n_bits);
    rem = MidAPInt_zero(a->n_bits);
    bignum_div(a->v.words, a_words, b->v.words, b_words, quot.v.words,
               rem.v.words);

    // holy spaghetti
finish_normal:
    // if a or b is going to be overwritten then we need to deinit them first
    if (a == out_quot || a == out_rem)
        MidAPInt_deinit((struct Mid_APInt *)a);

    if (b == out_quot || b == out_rem)
        MidAPInt_deinit((struct Mid_APInt *)b);

    *out_quot = quot;
    *out_rem = rem;

    return;

finish_cpy_a_to_quot:
    if (b == out_quot || b == out_rem)
        MidAPInt_deinit((struct Mid_APInt *)b);

    if (out_quot != a)
        *out_quot = MidAPInt_copy(a);
    *out_rem = rem;

    return;

finish_cpy_a_to_rem:
    if (b == out_quot || b == out_rem)
        MidAPInt_deinit((struct Mid_APInt *)b);

    if (out_rem != a)
        *out_rem = MidAPInt_copy(a);
    *out_quot = quot;

    return;
}

void MidAPInt_sdivrem(const struct Mid_APInt *a, const struct Mid_APInt *b,
                      struct Mid_APInt *out_quot, struct Mid_APInt *out_rem)
{
    bool a_neg = MidAPInt_get_sign_bit(a);
    bool b_neg = MidAPInt_get_sign_bit(b);

    if (a_neg && b_neg) {
        // -a / -b = +c
        // -a % -b = -c
        auto pos_a = MidAPInt_nip_negate(a);
        auto pos_b = MidAPInt_nip_negate(b);
        MidAPInt_udivrem(&pos_a, &pos_b, out_quot, out_rem);
        MidAPInt_negate(out_rem);

        MidAPInt_deinit(&pos_a);
        MidAPInt_deinit(&pos_b);
    } else if (a_neg) {
        // -a / +b = -c
        // -a % +b = -c
        auto pos_a = MidAPInt_nip_negate(a);
        MidAPInt_udivrem(&pos_a, b, out_quot, out_rem);
        MidAPInt_negate(out_quot);
        MidAPInt_negate(out_rem);

        MidAPInt_deinit(&pos_a);
    } else if (b_neg) {
        // +a / -b = -c
        // +a % -b = +c
        auto pos_b = MidAPInt_nip_negate(b);
        MidAPInt_udivrem(a, &pos_b, out_quot, out_rem);
        MidAPInt_negate(out_quot);

        MidAPInt_deinit(&pos_b);
    } else {
        // +a / +b = +c
        // +a % +b = +c
        MidAPInt_udivrem(a, b, out_quot, out_rem);
    }
}

void MidAPInt_not(struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = 0; i < get_n_words(self->n_bits); ++i)
            self->v.words[i] = ~self->v.words[i];
    } else {
        self->v.val = ~self->v.val;
    }

    MidAPInt_mask_extra_bits(self);
}

struct Mid_APInt MidAPInt_nip_not(const struct Mid_APInt *self)
{
    auto res = MidAPInt_copy(self);
    MidAPInt_not(&res);
    return res;
}

void MidAPInt_negate(struct Mid_APInt *self)
{
    MidAPInt_not(self);
    MidAPInt_add_imm(self, 1);
}

struct Mid_APInt MidAPInt_nip_negate(const struct Mid_APInt *self)
{
    auto res = MidAPInt_copy(self);
    MidAPInt_negate(&res);
    return res;
}

static bool is_word_pow2(MidAPInt_Word word)
{
    if (word == 0)
        return false;
    return (word & (word - 1)) == 0;
}

bool MidAPInt_is_pow2(const struct Mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        bool found_active = false;
        for (i32 i = 0; i < get_n_words(self->n_bits); ++i) {
            auto word = self->v.words[i];

            if (word == 0)
                continue;

            if (found_active)
                return false;
            found_active = true;

            if (!is_word_pow2(word))
                return false;
        }

        // if found_active is false then self == 0
        return found_active;
    } else {
        return is_word_pow2(self->v.val);
    }
}

bool MidAPInt_is_eq(const struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = 0; i < get_n_words(a->n_bits); ++i) {
            if (a->v.words[i] != b->v.words[i])
                return false;
        }

        return true;
    } else {
        return a->v.val == b->v.val;
    }
}

bool MidAPInt_is_ugt(const struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = get_n_words(a->n_bits) - 1; i >= 0; --i) {
            auto a_w = a->v.words[i];
            auto b_w = b->v.words[i];

            if (a_w == 0 && b_w == 0)
                continue;
            else if (a_w > b_w)
                return true;
            else
                return false;
        }

        return false;
    } else {
        return a->v.val > b->v.val;
    }
}

bool MidAPInt_is_ugteq(const struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = get_n_words(a->n_bits) - 1; i >= 0; --i) {
            auto a_w = a->v.words[i];
            auto b_w = b->v.words[i];

            if (a_w == 0 && b_w == 0)
                continue;
            else if (a_w >= b_w)
                return true;
            else
                return false;
        }

        return false;
    } else {
        return a->v.val > b->v.val;
    }
}

bool MidAPInt_is_ult(const struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = get_n_words(a->n_bits) - 1; i >= 0; --i) {
            auto a_w = a->v.words[i];
            auto b_w = b->v.words[i];

            if (a_w == 0 && b_w == 0)
                continue;
            else if (a_w < b_w)
                return true;
            else
                return false;
        }

        return false;
    } else {
        return a->v.val > b->v.val;
    }
}

bool MidAPInt_is_ulteq(const struct Mid_APInt *a, const struct Mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = get_n_words(a->n_bits) - 1; i >= 0; --i) {
            auto a_w = a->v.words[i];
            auto b_w = b->v.words[i];

            if (a_w == 0 && b_w == 0)
                continue;
            else if (a_w <= b_w)
                return true;
            else
                return false;
        }

        return false;
    } else {
        return a->v.val > b->v.val;
    }
}

static MidAPInt_Word clear_word_bits(MidAPInt_Word word, int lo, int hi)
{
    assert(lo >= 0 && lo < MidAPInt_word_n_bits);
    assert(hi > 0 && hi <= MidAPInt_word_n_bits);
    assert(hi > lo);

    int n = hi - lo;
    if (n == MidAPInt_word_n_bits)
        return 0;

    MidAPInt_Word mask = ((1 << n) - 1) << lo;
    return word & mask;
}

void MidAPInt_clear_bits(struct Mid_APInt *self, i32 lo, i32 hi)
{
    assert(lo >= 0 && lo < self->n_bits);
    assert(hi > 0 && hi <= self->n_bits);
    assert(hi > lo);

    if (is_bignum_used(self->n_bits)) {
        // make hi inclusive
        --hi;

        i32 start_word = get_n_words(lo) - 1;
        i32 end_word = get_n_words(hi) - 1;

        // clear all the words in between
        for (i32 i = start_word + 1; i < end_word - 1; ++i)
            self->v.words[i] = 0;

        i32 start_base = start_word * MidAPInt_word_n_bits;
        i32 end_base = end_word * MidAPInt_word_n_bits;
        // base of the word after start_base
        i32 start_next = start_base + MidAPInt_word_n_bits;

        i32 start_lo = lo - start_base;
        i32 start_hi =
            hi >= start_next ? MidAPInt_word_n_bits : hi - start_base;

        i32 end_lo = lo < end_base ? 0 : lo - end_base;
        i32 end_hi = hi - end_base;

        self->v.words[start_word] =
            clear_word_bits(self->v.words[start_word], start_lo, start_hi);
        if (end_word != start_word)
            self->v.words[end_word] =
                clear_word_bits(self->v.words[end_word], end_lo, end_hi);
    } else {
        self->v.val = clear_word_bits(self->v.val, lo, hi);
    }
}
