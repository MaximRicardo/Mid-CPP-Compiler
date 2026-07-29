#include "apint.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
// maximum nr of decimal digits to represent an unsigned int of n_bits width
static isize_t max_dec_digits(i32 n_bits)
{
    return ceil(n_bits * log10(2.0));
}
*/

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
        i32 word_idx = n_words_in_bits(n + 1);
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

// static void log_unsigned_words(const APInt_Word *words, i32 n, FILE *out) {}

static void log_uint(const struct APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = n_words_in_bits(self->n_bits) - 1; i >= 0; --i) {
            if (self->v.words[i] == 0 && i > 0)
                continue;
            fprintf(out, "%" APINT_WORD_UNSIGNED_FORMAT, self->v.words[i]);
        }
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
