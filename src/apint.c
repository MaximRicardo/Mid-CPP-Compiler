#include "apint.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// maximum nr of decimal digits to represent an unsigned int of n_bits width
static isize_t max_dec_digits(i32 n_bits)
{
    return ceil(n_bits * log10(2.0));
}

static APInt_Word uint_max_val(i32 n_bits)
{
    assert(n_bits > 0 && n_bits <= APInt_Word_n_bits);

    // avoids overflow if n_bits == APInt_Word_n_bits
    APInt_Word ret = 1ULL << (n_bits - 1);
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
    return ((n_bits - 1) % APInt_Word_n_bits) + 1;
}

/*
static APInt_Word last_word_max_val(i32 n_bits)
{
    return uint_max_val(n_bits_in_last_word(n_bits));
}
*/

static i32 n_words_in_bits(i32 n_bits)
{
    return (n_bits + (APInt_Word_n_bits - 1)) / APInt_Word_n_bits;
}

static bool is_bignum_used(i32 n_bits)
{
    return n_bits > APInt_Word_n_bits;
}

static APInt_Word mask_extra_bits(APInt_Word word, int n_bits)
{
    if (n_bits == APInt_Word_n_bits) {
        return word;
    } else {
        APInt_Word mask = (1ULL << n_bits) - 1;
        return word & mask;
    }
}

static APInt_Word sign_ext_word(APInt_Word word, int old_n_bits, int new_n_bits)
{
    APInt_Word m = 1ULL << (old_n_bits - 1);
    APInt_Word ret = (word ^ m) - m;
    return mask_extra_bits(ret, new_n_bits);
}

void APInt_deinit(struct APInt *self)
{
    if (is_bignum_used(self->n_bits))
        free(self->v.words);
}

struct APInt APInt_init_arr(i32 n_bits, const APInt_Word *words, i32 n_words,
                            bool sign_ext)
{
    assert(n_words > 0);

    if (!is_bignum_used(n_bits)) {
        return APInt_init_no_limit_check(n_bits, words[0], sign_ext);
    }

    i32 dest_n_words = n_words_in_bits(n_bits);

    struct APInt ret = {.n_bits = n_bits};
    ret.v.words = mid_malloc(dest_n_words * sizeof(*ret.v.words));

    for (i32 i = 0; i < dest_n_words; ++i) {
        if (i < n_words)
            ret.v.words[i] = words[i];
        else
            ret.v.words[i] = sign_ext ? APInt_Word_max : 0;
    }

    auto last = &ret.v.words[dest_n_words - 1];
    *last = mask_extra_bits(*last, n_bits_in_last_word(ret.n_bits));

    return ret;
}

static struct APInt apint_init_impl(i32 n_bits, APInt_Word val, bool is_signed,
                                    bool limit_check)
{
    assert(n_bits > 0);

    if (is_bignum_used(n_bits)) {
        return APInt_init_arr(n_bits, &val, 1, is_signed);
    }

    if (limit_check) {
        if (is_signed) {
            long long s_val = val;
            if (s_val > sint_max_val(n_bits) || s_val < sint_min_val(n_bits))
                CRASH("signed val doesn't fit in n_bits");
        } else {
            if (val > uint_max_val(n_bits))
                CRASH("unsigned val doesn't fit in n_bits");
        }
    }

    return (struct APInt){.n_bits = n_bits,
                          .v.val = mask_extra_bits(val, n_bits)};
}

struct APInt APInt_init(i32 n_bits, APInt_Word val, bool is_signed)
{
    return apint_init_impl(n_bits, val, is_signed, true);
}

struct APInt APInt_init_no_limit_check(i32 n_bits, APInt_Word val,
                                       bool is_signed)
{
    return apint_init_impl(n_bits, val, is_signed, false);
}

struct APInt APInt_zero(i32 n_bits)
{
    struct APInt ret = {.n_bits = n_bits};

    if (!is_bignum_used(n_bits))
        ret.v.val = 0;
    else
        ret.v.words = mid_calloc(n_words_in_bits(n_bits), sizeof(*ret.v.words));

    return ret;
}

struct APInt APInt_copy(const struct APInt *src)
{
    struct APInt dest = {.n_bits = src->n_bits};

    if (is_bignum_used(src->n_bits)) {
        size_t size = n_words_in_bits(src->n_bits) * sizeof(*src->v.words);
        dest.v.words = mid_malloc(size);
        memcpy(dest.v.words, src->v.words, size);
    } else {
        dest.v.val = src->v.val;
    }

    return dest;
}

bool APInt_is_zero(const struct APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = 0; i < n_words_in_bits(self->n_bits); ++i) {
            if (self->v.words[i] != 0)
                return false;
        }

        return true;
    } else {
        return self->v.val == 0;
    }
}

void APInt_ext(struct APInt *self, i32 new_n_bits, bool sign_ext)
{
    bool old_uses_bignum = is_bignum_used(self->n_bits);
    bool new_uses_bignum = is_bignum_used(new_n_bits);

    if (old_uses_bignum && new_uses_bignum) {
        i32 n_words = n_words_in_bits(self->n_bits);
        i32 n_new_words = n_words_in_bits(new_n_bits);
        self->v.words =
            mid_realloc(self->v.words, n_new_words * sizeof(*self->v.words));

        if (new_n_bits < self->n_bits) {
            auto last = &self->v.words[n_new_words - 1];
            *last = mask_extra_bits(*last, new_n_bits);
        } else if (n_new_words > n_words) {
            for (auto i = n_words; i < n_new_words; ++i) {
                self->v.words[i] = sign_ext ? APInt_Word_max : 0;
            }
        }
    } else if (old_uses_bignum) {
        auto lsw = self->v.words[0];
        free(self->v.words);
        self->v.val = mask_extra_bits(lsw, new_n_bits);
    } else if (new_uses_bignum) {
        auto lsw = self->v.val;
        if (sign_ext)
            lsw = sign_ext_word(lsw, self->n_bits, APInt_Word_n_bits);
        *self = APInt_init(new_n_bits, lsw, sign_ext);
    } else {
        if (new_n_bits > self->n_bits && sign_ext)
            self->v.val = sign_ext_word(self->v.val, self->n_bits, new_n_bits);
        else
            self->v.val = mask_extra_bits(self->v.val, new_n_bits);
    }

    self->n_bits = new_n_bits;
}

bool APInt_get_bit(const struct APInt *self, i32 n)
{
    assert(n > 0 && n < self->n_bits);

    if (is_bignum_used(self->n_bits)) {
        i32 word_idx = n_words_in_bits(n + 1) - 1;
        const APInt_Word *word = &self->v.words[word_idx];

        i32 n_in_word = n_bits_in_last_word(n + 1);
        return (*word >> n_in_word) & 1;
    } else {
        return (self->v.val >> n) & 1;
    }
}

bool APInt_get_sign_bit(const struct APInt *self)
{
    return APInt_get_bit(self, self->n_bits - 1);
}

static void log_uint_bignum(const struct APInt *self, FILE *out)
{
    auto tmp = APInt_copy(self);
    auto ten = APInt_init(tmp.n_bits, 10, false);

    char *digits = mid_malloc(max_dec_digits(self->n_bits) * sizeof(*digits));

    isize_t i = 0;
    while (APInt_is_gteq(&tmp, &ten)) {
        auto d = APInt_nip_urem(&tmp, &ten);
        // d is guaranteed to be less than 10 so we can just take it from
        // the first word
        digits[i] = d.v.words[0] + '0';

        APInt_udiv(&tmp, &ten);

        APInt_deinit(&d);
        ++i;
    }

    auto d = APInt_nip_urem(&tmp, &ten);
    digits[i] = d.v.words[0] + '0';

    for (; i >= 0; --i) {
        fputc(digits[i], out);
    }

    free(digits);
    APInt_deinit(&d);
    APInt_deinit(&ten);
    APInt_deinit(&tmp);
}

static void log_uint(const struct APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        /*
        for (i32 i = n_words_in_bits(self->n_bits) - 1; i >= 0; --i) {
            if (self->v.words[i] == 0 && i > 0)
                continue;
            fprintf(out, "%" APINT_WORD_UNSIGNED_FORMAT, self->v.words[i]);
        }
        */
        log_uint_bignum(self, out);
    } else {
        fprintf(out, "%" APINT_WORD_UNSIGNED_FORMAT, self->v.val);
    }
}

void APInt_log(const struct APInt *self, FILE *out, bool is_signed)
{
    if (is_signed)
        ;
    else
        log_uint(self, out);
}

void APInt_log_hex(const struct APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        bool printed = false;
        for (i32 i = n_words_in_bits(self->n_bits) - 1; i >= 1; --i) {
            if (self->v.words[i] == 0)
                continue;
            printed = true;
            fprintf(out, "%" APINT_WORD_HEX_FORMAT, self->v.words[i]);
        }

        if (printed)
            fprintf(out, "%" APINT_WORD_FULL_HEX_FORMAT, self->v.words[0]);
        else
            fprintf(out, "%" APINT_WORD_HEX_FORMAT, self->v.words[0]);
    } else {
        fprintf(out, "%" APINT_WORD_HEX_FORMAT, self->v.val);
    }
}

void APInt_add(struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        bool carry = false;
        for (i32 i = 0; i < n_words_in_bits(a->n_bits); ++i) {
            auto old = a->v.words[i];

            a->v.words[i] += b->v.words[i];

            // the word might have wrapped around or might be about to wrap
            // around due to the previous carry
            bool new_carry = a->v.words[i] < old ||
                             (carry && a->v.words[i] == APInt_Word_max);

            a->v.words[i] += carry;

            carry = new_carry;
        }
    } else {
        a->v.val += b->v.val;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }
}

static void shl_bignum_case(struct APInt *a, i32 count)
{
    i32 word_shift = count / APInt_Word_n_bits;
    i32 bit_shift = count % APInt_Word_n_bits;

    i32 n_words = n_words_in_bits(a->n_bits);

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
                                 (APInt_Word_n_bits - bit_shift);
        }
    }

    // fill the remainder with 0s
    memset(a->v.words, 0, word_shift * sizeof(*a->v.words));
}

static void lshr_bignum_case(struct APInt *a, i32 count)
{
    i32 word_shift = count / APInt_Word_n_bits;
    i32 bit_shift = count % APInt_Word_n_bits;

    i32 n_words = n_words_in_bits(a->n_bits);

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
                                 << (APInt_Word_n_bits - bit_shift);
        }
    }

    // fill the remainder with 0s
    memset(&a->v.words[n_words - word_shift], 0,
           word_shift * sizeof(*a->v.words));
}

void APInt_shl(struct APInt *a, i32 count)
{
    assert(count >= 0 && count < a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        shl_bignum_case(a, count);
    } else {
        a->v.val <<= count;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }
}

void APInt_lshr(struct APInt *a, i32 count)
{
    assert(count >= 0 && count < a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        lshr_bignum_case(a, count);
    } else {
        a->v.val >>= count;
    }
}

/*
 * u is the dividend (lhs) and is m + n digits long
 * v is the divisor (rhs) and is n digits long
 * q = u / v
 * r (optional) = u % v;
 */
static void knuth_bignum_div(u32 *u, u32 *v, u32 *q, u32 *r, u32 m, u32 n)
{
    // i have no clue how any of this works ngl

    u64 b = UINT32_MAX;
    auto d = (b - 1) % v[n - 1];

    for (u32 i = 0; i < m + n; ++i)
        u[i] *= d;
    for (u32 i = 0; i < n; ++i)
        v[i] *= d;

    for (u32 j = m; j >= 0; --j) {
        u64 x = u[n + j] * b + u[n - 1 + j];
        u64 q_hat = x / v[n - 1];
        u64 r_hat = x % v[n - 1];

        do {
            if (q_hat == b || q_hat * v[n - 2] > r_hat * b + u[n - 2 + j]) {
                --q_hat;
                r_hat += v[n - 1];
            }
        } while (r_hat < b);

        // multiply and subtract
        u64 k = 0;
        for (u32 i = 0; i < n; ++i) {
            u64 p = q_hat * v[i];
            u64 t = u[i + j] - k - (p & UINT32_MAX);
            u[i + j] = t;
            k = (p >> 32) - (t >> 32);
        }
        u64 t = u[j + n] - k;
        u[j + n] = t;

        q[j] = q_hat;
        if (t < 0) {
            --q[j];
            k = 0;
            for (u32 i = 0; i < n; ++i) {
                t = (u64)u[i + j] + v[i] + k;
                u[i + j] = t;
                k = t >> 32;
            }
            u[j + n] += k;
        }

        if (r) {
            for (u32 i = 0; i < n - 1; ++i) {
                r[i] = u[i] / d;
            }
        }
    }
}

static void convert_to_u32_digits(const APInt_Word *words, i32 n_words,
                                  u32 *digits)
{
    for (i32 i = 0; i < n_words; ++i) {
        digits[i * 2] = words[i];
        digits[i * 2 + 1] = words[i] >> 32;
    }
}

// i would like to say i understand any of this but i really dont
static void bignum_div(const APInt_Word *a, i32 a_n_words, const APInt_Word *b,
                       i32 b_n_words, APInt_Word *out_quot, APInt_Word *out_rem)
{
    assert(a_n_words >= b_n_words);

    // assumes APInt_Word is 64 bits
    static_assert(sizeof(APInt_Word) == sizeof(u64));

    i32 n = b_n_words * 2;
    i32 m = a_n_words * 2 - n;

    u32 *u = mid_malloc((m + n + 1) * sizeof(*u));
    u32 *v = mid_malloc(n * sizeof(*v));
    u32 *q = mid_malloc((m + n) * sizeof(*q));
    u32 *r = out_rem ? mid_malloc(n * sizeof(*r)) : NULL;

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
        CRASH("division by 0");

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

struct APInt APInt_nip_udiv(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    i32 n_words = n_words_in_bits(a->n_bits);
    if (n_words == 1) {
        return APInt_init(a->n_bits, a->v.val / b->v.val, false);
    } else {
        // FIXME: FIND NUMBER OF WORDS B ACTUALLY USES
        struct APInt quot = APInt_zero(a->n_bits);
        bignum_div(a->v.words, n_words, b->v.words, n_words, quot.v.words,
                   NULL);
        return quot;
    }
}

void APInt_udiv(struct APInt *a, const struct APInt *b)
{
    auto tmp = APInt_nip_udiv(a, b);
    APInt_deinit(a);
    *a = tmp;
}

void APInt_urem(struct APInt *a, const struct APInt *b)
{
    auto tmp = APInt_nip_urem(a, b);
    APInt_deinit(a);
    *a = tmp;
}

struct APInt APInt_nip_urem(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    i32 n_words = n_words_in_bits(a->n_bits);
    if (n_words == 1) {
        return APInt_init(a->n_bits, a->v.val % b->v.val, false);
    } else {
        struct APInt rem = APInt_zero(a->n_bits);
        bignum_div(a->v.words, n_words, b->v.words, n_words, NULL, rem.v.words);
        return rem;
    }
}

bool APInt_is_eq(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = 0; i < n_words_in_bits(a->n_bits); ++i) {
            if (a->v.words[i] != b->v.words[i])
                return false;
        }

        return true;
    } else {
        return a->v.val == b->v.val;
    }
}

bool APInt_is_gt(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = n_words_in_bits(a->n_bits) - 1; i >= 0; --i) {
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

bool APInt_is_gteq(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = n_words_in_bits(a->n_bits) - 1; i >= 0; --i) {
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

bool APInt_is_lt(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = n_words_in_bits(a->n_bits) - 1; i >= 0; --i) {
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

bool APInt_is_lteq(const struct APInt *a, const struct APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = n_words_in_bits(a->n_bits) - 1; i >= 0; --i) {
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
