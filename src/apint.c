#include "apint.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

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

static int countl_zero_word(midint_Word num)
{
    if (num == 0)
        return midint_word_n_bits;

    int zeroes = 0;
    for (u32 shift = midint_word_n_bits / 2; shift; shift >>= 1) {
        auto tmp = num >> shift;
        if (tmp)
            num = tmp;
        else
            zeroes |= shift;
    }

    return zeroes;
}

static int countl_one_word(midint_Word num)
{
    return countl_zero_word(~num);
}

static int unsigned_sig_bits(midint_Word word)
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

static midint_Word uint_max_val(i32 n_bits)
{
    assert(n_bits > 0 && n_bits <= midint_word_n_bits);

    // avoids overflow if n_bits == midint_word_n_bits
    midint_Word ret = 1ULL << (n_bits - 1);
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
    return ((n_bits - 1) % midint_word_n_bits) + 1;
}

/*
static midint_Word last_word_max_val(i32 n_bits)
{
    return uint_max_val(n_bits_in_last_word(n_bits));
}
*/

static i32 get_n_words(i32 n_bits)
{
    return (n_bits + (midint_word_n_bits - 1)) / midint_word_n_bits;
}

static bool is_bignum_used(i32 n_bits)
{
    return n_bits > midint_word_n_bits;
}

static midint_Word mask_extra_bits(midint_Word word, int n_bits)
{
    if (n_bits == midint_word_n_bits) {
        return word;
    } else {
        midint_Word mask = (1ULL << n_bits) - 1;
        return word & mask;
    }
}

static midint_Word sign_ext_word(midint_Word word, int old_n_bits,
                                 int new_n_bits)
{
    midint_Word m = 1ULL << (old_n_bits - 1);
    midint_Word ret = (word ^ m) - m;
    return mask_extra_bits(ret, new_n_bits);
}

void mid_APInt_deinit(struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits))
        free(self->v.words);
}

struct mid_APInt midint_init_arr(i32 n_bits, const midint_Word *words,
                                 i32 n_words, bool sign_ext)
{
    assert(n_words > 0);

    if (!is_bignum_used(n_bits)) {
        return midint_init_no_limit_check(n_bits, words[0], sign_ext);
    }

    bool is_signed = words[n_words - 1] & (1ULL << (midint_word_n_bits - 1));

    i32 dest_n_words = get_n_words(n_bits);

    struct mid_APInt ret = {.n_bits = n_bits};
    ret.v.words = mid_malloc(dest_n_words * sizeof(*ret.v.words));

    for (i32 i = 0; i < dest_n_words; ++i) {
        if (i < n_words)
            ret.v.words[i] = words[i];
        else
            ret.v.words[i] = (sign_ext && is_signed) ? midint_word_max : 0;
    }

    auto last = &ret.v.words[dest_n_words - 1];
    *last = mask_extra_bits(*last, n_bits_in_last_word(ret.n_bits));

    return ret;
}

static struct mid_APInt apint_init_impl(i32 n_bits, midint_Word val,
                                        bool is_signed, bool limit_check)
{
    assert(n_bits > 0);

    if (is_bignum_used(n_bits)) {
        return midint_init_arr(n_bits, &val, 1, is_signed);
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

    return (struct mid_APInt){.n_bits = n_bits,
                              .v.val = mask_extra_bits(val, n_bits)};
}

struct mid_APInt midint_init(i32 n_bits, midint_Word val, bool is_signed)
{
    return apint_init_impl(n_bits, val, is_signed, true);
}

struct mid_APInt midint_init_no_limit_check(i32 n_bits, midint_Word val,
                                            bool is_signed)
{
    return apint_init_impl(n_bits, val, is_signed, false);
}

struct mid_APInt midint_alloc(i32 n_bits)
{
    struct mid_APInt ret = {.n_bits = n_bits};

    if (is_bignum_used(n_bits))
        ret.v.words = mid_malloc(get_n_words(n_bits) * sizeof(*ret.v.words));

    return ret;
}

struct mid_APInt midint_zero(i32 n_bits)
{
    struct mid_APInt ret = {.n_bits = n_bits};

    if (!is_bignum_used(n_bits))
        ret.v.val = 0;
    else
        ret.v.words = mid_calloc(get_n_words(n_bits), sizeof(*ret.v.words));

    return ret;
}

struct mid_APInt midint_one(i32 n_bits)
{
    return midint_init(n_bits, 1, false);
}

struct mid_APInt midint_copy(const struct mid_APInt *src)
{
    struct mid_APInt dest = {.n_bits = src->n_bits};

    if (is_bignum_used(src->n_bits)) {
        size_t size = get_n_words(src->n_bits) * sizeof(*src->v.words);
        dest.v.words = mid_malloc(size);
        memcpy(dest.v.words, src->v.words, size);
    } else {
        dest.v.val = src->v.val;
    }

    return dest;
}

void midint_assign(struct mid_APInt *dest, const struct mid_APInt *src)
{
    assert(dest->n_bits == src->n_bits);

    if (dest == src)
        return;

    if (is_bignum_used(dest->n_bits)) {
        memcpy(dest->v.words, src->v.words,
               get_n_words(dest->n_bits) * sizeof(*dest->v.words));
    } else {
        dest->v.val = src->v.val;
    }
}

void midint_assign_uimm(struct mid_APInt *dest, u64 src)
{
    if (is_bignum_used(dest->n_bits)) {
        dest->v.words[0] = src;
        for (i32 i = 1; i < get_n_words(dest->n_bits); ++i)
            dest->v.words[i] = 0;
    } else {
        dest->v.val = src;
        midint_mask_extra_bits(dest);
    }
}

void midint_assign_simm(struct mid_APInt *dest, i64 src)
{
    if (is_bignum_used(dest->n_bits)) {
        dest->v.words[0] = src;
        for (i32 i = 1; i < get_n_words(dest->n_bits); ++i)
            dest->v.words[i] = src < 0 ? midint_word_n_bits : 0;
    } else {
        dest->v.val = src;
        midint_mask_extra_bits(dest);
    }
}

bool midint_is_zero(const struct mid_APInt *self)
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

static bool is_word_signed_min(midint_Word word, int n_bits)
{
    if (word == 0)
        return false;

    // mask away the sign bit
    // if the result is zero then that was the only active bit and the value
    // is the signed minimum
    return (word & ~(1ULL << (n_bits - 1))) == 0;
}

bool midint_is_signed_min(const struct mid_APInt *self)
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

static bool word_is_all_ones(midint_Word word, int n_bits)
{
    if (n_bits == midint_word_n_bits) {
        return word == midint_word_max;
    } else {
        // set all unused bits high
        midint_Word mask = ~((1ULL << n_bits) - 1);
        word = word | mask;

        return word == midint_word_max;
    }
}

bool midint_is_all_ones(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);
        if (!word_is_all_ones(self->v.words[n_words - 1],
                              n_bits_in_last_word(self->n_bits)))
            return false;

        for (i32 i = 0; i < n_words - 2; ++i) {
            if (self->v.words[i] != midint_word_max)
                return false;
        }

        return true;
    } else {
        return word_is_all_ones(self->v.val, self->n_bits);
    }
}

bool midint_is_umax(const struct mid_APInt *self)
{
    return midint_is_all_ones(self);
}

bool midint_is_smax(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);
        midint_Word sign_bit = 1ULL << (n_bits_in_last_word(self->n_bits) - 1);
        if (self->v.words[n_words - 1] >= sign_bit)
            return false;

        for (i32 i = 0; i < n_words - 2; ++i) {
            if (self->v.words[i] != midint_word_max)
                return false;
        }

        return true;
    } else {
        midint_Word sign_bit = (1ULL << (self->n_bits - 1));
        return self->v.val < sign_bit;
    }
}

bool midint_is_smin(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);
        midint_Word sign_bit = 1ULL << (n_bits_in_last_word(self->n_bits) - 1);
        if (self->v.words[n_words - 1] == sign_bit)
            return false;

        for (i32 i = 0; i < n_words - 2; ++i) {
            if (self->v.words[i] != 0)
                return false;
        }

        return true;
    } else {
        midint_Word sign_bit = (1ULL << (self->n_bits - 1));
        return self->v.val == sign_bit;
    }
}

void midint_ext(struct mid_APInt *self, i32 new_n_bits, bool sign_ext)
{
    bool old_uses_bignum = is_bignum_used(self->n_bits);
    bool new_uses_bignum = is_bignum_used(new_n_bits);

    if (old_uses_bignum && new_uses_bignum) {
        i32 n_words = get_n_words(self->n_bits);
        i32 n_new_words = get_n_words(new_n_bits);
        self->v.words =
            mid_realloc(self->v.words, n_new_words * sizeof(*self->v.words));

        if (new_n_bits < self->n_bits) {
            auto last = &self->v.words[n_new_words - 1];
            *last = mask_extra_bits(*last, new_n_bits);
        } else if (n_new_words > n_words) {
            for (auto i = n_words; i < n_new_words; ++i) {
                self->v.words[i] = sign_ext ? midint_word_max : 0;
            }
        }
    } else if (old_uses_bignum) {
        auto lsw = self->v.words[0];
        free(self->v.words);
        self->v.val = mask_extra_bits(lsw, new_n_bits);
    } else if (new_uses_bignum) {
        auto lsw = self->v.val;
        if (sign_ext)
            lsw = sign_ext_word(lsw, self->n_bits, midint_word_n_bits);
        *self = midint_init(new_n_bits, lsw, sign_ext);
    } else {
        if (new_n_bits > self->n_bits && sign_ext)
            self->v.val = sign_ext_word(self->v.val, self->n_bits, new_n_bits);
        else
            self->v.val = mask_extra_bits(self->v.val, new_n_bits);
    }

    self->n_bits = new_n_bits;
}

bool midint_get_bit(const struct mid_APInt *self, i32 n)
{
    assert(n >= 0 && n < self->n_bits);

    if (is_bignum_used(self->n_bits)) {
        i32 word_idx = get_n_words(n + 1) - 1;
        const midint_Word *word = &self->v.words[word_idx];

        i32 n_in_word = n - word_idx * midint_word_n_bits;
        return (*word >> n_in_word) & 1;
    } else {
        return (self->v.val >> n) & 1;
    }
}

bool midint_get_sign_bit(const struct mid_APInt *self)
{
    return midint_get_bit(self, self->n_bits - 1);
}

static void log_uint_bignum(const struct mid_APInt *self, FILE *out)
{
    auto tmp = midint_copy(self);
    auto ten = midint_init(tmp.n_bits, 10, false);
    auto d = midint_alloc(tmp.n_bits);

    char *digits = mid_malloc(max_dec_digits(self->n_bits) * sizeof(*digits));

    mid_isize i = 0;
    while (midint_is_ugteq(&tmp, &ten)) {
        midint_udivrem(&tmp, &ten, &tmp, &d);

        // d is guaranteed to be less than 10 so we can just take it from
        // the first word
        digits[i] = d.v.words[0] + '0';

        ++i;
    }

    mid_APInt_deinit(&d);
    d = midint_nip_urem(&tmp, &ten);
    digits[i] = d.v.words[0] + '0';

    for (; i >= 0; --i) {
        fputc(digits[i], out);
    }

    free(digits);
    mid_APInt_deinit(&d);
    mid_APInt_deinit(&ten);
    mid_APInt_deinit(&tmp);
}

static void log_uint(const struct mid_APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        log_uint_bignum(self, out);
    } else {
        fprintf(out, "%" MIDINT_WORD_UNSIGNED_FORMAT, self->v.val);
    }
}

static void log_sint(const struct mid_APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        bool is_negative = midint_get_sign_bit(self);
        if (!is_negative) {
            log_uint_bignum(self, out);
        } else if (midint_is_signed_min(self)) {
            // we can't negate self without overflowing so we need to
            // allocate an extra bit
            assert(self->n_bits < INT32_MAX);

            fputc('-', out);

            auto tmp = midint_copy(self);
            midint_ext(&tmp, self->n_bits + 1, true);

            midint_negate(&tmp);
            log_uint_bignum(&tmp, out);

            mid_APInt_deinit(&tmp);
        } else {
            // negate the number and print its unsigned version
            fputc('-', out);

            auto tmp = midint_copy(self);
            midint_negate(&tmp);
            log_uint_bignum(&tmp, out);
            mid_APInt_deinit(&tmp);
        }
    } else {
        fprintf(out, "%" MIDINT_WORD_SIGNED_FORMAT,
                sign_ext_word(self->v.val, self->n_bits, midint_word_n_bits));
    }
}

void midint_log(const struct mid_APInt *self, FILE *out, bool is_signed)
{
    if (is_signed)
        log_sint(self, out);
    else
        log_uint(self, out);
}

void midint_log_hex(const struct mid_APInt *self, FILE *out)
{
    if (is_bignum_used(self->n_bits)) {
        bool printed = false;
        for (i32 i = get_n_words(self->n_bits) - 1; i >= 1; --i) {
            if (self->v.words[i] == 0)
                continue;
            printed = true;
            fprintf(out, "%" MIDINT_WORD_HEX_FORMAT, self->v.words[i]);
        }

        if (printed)
            fprintf(out, "%" MIDINT_WORD_FULL_HEX_FORMAT, self->v.words[0]);
        else
            fprintf(out, "%" MIDINT_WORD_HEX_FORMAT, self->v.words[0]);
    } else {
        fprintf(out, "%" MIDINT_WORD_HEX_FORMAT, self->v.val);
    }
}

i32 midint_unsigned_sig_bits(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = get_n_words(self->n_bits) - 1; i >= 0; --i) {
            auto word = self->v.words[i];
            if (word != 0)
                return unsigned_sig_bits(word) + i * midint_word_n_bits;
        }

        return 0;
    } else {
        return unsigned_sig_bits(self->v.val);
    }
}

static i32 countl_zero(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        auto n_words = get_n_words(self->n_bits);

        i32 zeroes = 0;
        for (i32 i = n_words - 1; i >= 0; --i) {
            if (self->v.words[i] != 0) {
                zeroes += countl_zero_word(self->v.words[i]);
                return zeroes;
            }

            zeroes += midint_word_n_bits;
        }

        return self->n_bits;
    } else {
        return countl_zero_word(self->v.val);
    }
}

static i32 countl_one(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        auto n_words = get_n_words(self->n_bits);

        i32 ones = 0;
        for (i32 i = n_words - 1; i >= 0; --i) {
            if (self->v.words[i] != midint_word_max) {
                ones += countl_one_word(self->v.words[i]);
                return ones;
            }

            ones += midint_word_n_bits;
        }

        return self->n_bits;
    } else {
        return countl_one_word(self->v.val);
    }
}

i32 midint_signed_sig_bits(const struct mid_APInt *self)
{
    i32 n_sign_bits =
        midint_is_negative(self) ? countl_one(self) : countl_zero(self);

    return self->n_bits - n_sign_bits + 1;
}

void midint_mask_extra_bits(struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 last = get_n_words(self->n_bits) - 1;
        self->v.words[last] = mask_extra_bits(
            self->v.words[last], n_bits_in_last_word(self->n_bits));
    } else {
        self->v.val = mask_extra_bits(self->v.val, self->n_bits);
    }
}

void midint_add(struct mid_APInt *a, const struct mid_APInt *b)
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
                             (carry && a->v.words[i] == midint_word_max);

            a->v.words[i] += carry;

            carry = new_carry;
        }
    } else {
        a->v.val += b->v.val;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }

    midint_mask_extra_bits(a);
}

void midint_add_uimm(struct mid_APInt *a, u64 b)
{
    if (is_bignum_used(a->n_bits)) {
        a->v.words[0] += b;
        bool carry = a->v.words[0] < b;

        for (i32 i = 1; i < get_n_words(a->n_bits); ++i) {
            a->v.words[i] += carry;
            carry = carry && a->v.words[i] == midint_word_max;
        }
    } else {
        a->v.val += b;
        a->v.val = mask_extra_bits(a->v.val, a->n_bits);
    }

    midint_mask_extra_bits(a);
}

void midint_sub(struct mid_APInt *a, const struct mid_APInt *b)
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

    midint_mask_extra_bits(a);
}

void midint_sub_uimm(struct mid_APInt *a, u64 b)
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

    midint_mask_extra_bits(a);
}

static midint_Word get_low_half(midint_Word word)
{
    midint_Word mask = (1ULL << (midint_word_n_bits / 2)) - 1;
    return word & mask;
}

static midint_Word get_high_half(midint_Word word)
{
    return word >> (midint_word_n_bits / 2);
}

// lowkirkenuinelly no idea how this works, look at LLVM's APInt source code
// for documentation
static int tc_multiply_part(midint_Word *dst, const midint_Word *src,
                            midint_Word mul, midint_Word carry, i32 src_parts,
                            i32 dst_parts, bool add)
{
    assert(dst <= src || dst >= src + src_parts);
    assert(0 <= dst_parts);
    assert(dst_parts <= src_parts + 1);

    i32 n = MID_MIN(dst_parts, src_parts);

    for (i32 i = 0; i < n; ++i) {
        midint_Word src_part = src[i];
        midint_Word low, mid, high;
        if (mul == 0 || src_part == 0) {
            low = carry;
            high = 0;
        } else {
            low = get_low_half(src_part) * get_low_half(mul);
            high = get_high_half(src_part) * get_high_half(mul);

            mid = get_low_half(src_part) * get_high_half(mul);
            high += get_high_half(mid);
            mid <<= midint_word_n_bits / 2;
            if (low + mid < low)
                ++high;
            low += mid;

            mid = get_high_half(src_part) * get_low_half(mul);
            high += get_high_half(mid);
            mid <<= midint_word_n_bits / 2;
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
static int tc_multiply(midint_Word *restrict dst, const midint_Word *lhs,
                       const midint_Word *rhs, i32 parts)
{
    int overflow = 0;

    for (i32 i = 0; i < parts; ++i) {
        overflow |=
            tc_multiply_part(&dst[i], lhs, rhs[i], 0, parts, parts - i, i != 0);
    }

    return overflow;
}

// dst = lhs * rhs, where dst has the width of the sum of the widths of the
// operands.
// dst can not alias with lhs or rhs
static void tc_full_multiply(midint_Word *restrict dst, const midint_Word *lhs,
                             const midint_Word *rhs, i32 lhs_parts,
                             i32 rhs_parts)
{
    // the narrower number should be on the LHS for performance
    if (lhs_parts > rhs_parts) {
        tc_full_multiply(dst, rhs, lhs, rhs_parts, lhs_parts);
        return;
    }

    for (i32 i = 0; i < lhs_parts; ++i) {
        tc_multiply_part(&dst[i], rhs, lhs[i], 0, rhs_parts, rhs_parts + 1,
                         i != 0);
    }
}

void midint_ufullmul(const struct mid_APInt *a, const struct mid_APInt *b,
                     struct mid_APInt *out_res)
{
    assert(a->n_bits == b->n_bits);
    assert(out_res->n_bits == a->n_bits * 2);

    if (!is_bignum_used(out_res->n_bits)) {
        out_res->v.val = a->v.val * b->v.val;
    } else if (!is_bignum_used(a->n_bits)) {
        tc_full_multiply(out_res->v.words, &a->v.val, &b->v.val, 1, 1);
    } else {
        tc_full_multiply(out_res->v.words, a->v.words, b->v.words,
                         get_n_words(a->n_bits), get_n_words(b->n_bits));
    }
}

struct mid_APInt midint_nip_mul(const struct mid_APInt *a,
                                const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (!is_bignum_used(a->n_bits))
        return midint_init_no_limit_check(a->n_bits, a->v.val * b->v.val,
                                          false);

    auto res = midint_alloc(a->n_bits);
    tc_multiply(res.v.words, a->v.words, b->v.words, get_n_words(a->n_bits));
    midint_mask_extra_bits(&res);
    return res;
}

struct mid_APInt midint_nip_mul_uimm(const struct mid_APInt *a, u64 b)
{
    if (!is_bignum_used(a->n_bits)) {
        return midint_init_no_limit_check(a->n_bits, a->v.val * b, false);
    } else {
        auto n_words = get_n_words(a->n_bits);
        auto res = midint_alloc(a->n_bits);
        tc_multiply_part(res.v.words, a->v.words, b, 0, n_words, n_words,
                         false);
        return res;
    }
}

void midint_mul_uimm(struct mid_APInt *a, u64 b)
{
    if (!is_bignum_used(a->n_bits)) {
        a->v.val *= b;
    } else {
        auto n_words = get_n_words(a->n_bits);
        tc_multiply_part(a->v.words, a->v.words, b, 0, n_words, n_words, false);
    }

    midint_mask_extra_bits(a);
}

void midint_mul(struct mid_APInt *a, const struct mid_APInt *b)
{
    auto tmp = midint_nip_mul(a, b);
    mid_APInt_deinit(a);
    *a = tmp;
}

static void shl_bignum_case(struct mid_APInt *a, i32 count)
{
    i32 word_shift = count / midint_word_n_bits;
    i32 bit_shift = count % midint_word_n_bits;

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
                                 (midint_word_n_bits - bit_shift);
        }
    }

    // fill the remainder with 0s
    memset(a->v.words, 0, word_shift * sizeof(*a->v.words));

    midint_mask_extra_bits(a);
}

static void lshr_bignum_case(struct mid_APInt *a, i32 count)
{
    i32 word_shift = count / midint_word_n_bits;
    i32 bit_shift = count % midint_word_n_bits;

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
                                 << (midint_word_n_bits - bit_shift);
        }
    }

    // fill the remainder with 0s
    memset(&a->v.words[n_words - word_shift], 0,
           word_shift * sizeof(*a->v.words));

    midint_mask_extra_bits(a);
}

// guarantees arithmetic right shift even on systems where right shifting
// a signed integer is still logical
static u64 shift_arith_right(u64 val, unsigned sh)
{
    u64 mask = (1ULL << 63);
    u64 result = (val >> sh) | -((val & mask) >> sh);
    return result;
}

static void ashr_bignum_case(struct mid_APInt *a, i32 count)
{
    i32 word_shift = count / midint_word_n_bits;
    i32 bit_shift = count % midint_word_n_bits;

    i32 n_words = get_n_words(a->n_bits);
    i32 words_to_move = n_words - word_shift;

    bool is_neg = midint_get_sign_bit(a);

    if (words_to_move != 0) {
        // fill in the last word via sign extension
        a->v.words[n_words - 1] =
            sign_ext_word(a->v.words[n_words - 1],
                          n_bits_in_last_word(a->n_bits), midint_word_n_bits);

        if (bit_shift == 0) {
            // shortcut for shifting whole words
            memmove(a->v.words, &a->v.words[word_shift],
                    words_to_move * sizeof(*a->v.words));
        } else {
            // move the words containing significant bits
            for (i32 i = 0; i < words_to_move - 1; ++i) {
                a->v.words[i] = (a->v.words[i + word_shift] >> bit_shift) |
                                (a->v.words[i + word_shift + 1]
                                 << (midint_word_n_bits - bit_shift));
            }

            // the last word needs to be arithmetic right shifted
            a->v.words[words_to_move - 1] =
                shift_arith_right(a->v.words[words_to_move - 1], bit_shift);
        }
    }

    // fill the remainder with the sign bit
    memset(&a->v.words[n_words - word_shift], is_neg ? -1 : 0,
           word_shift * sizeof(*a->v.words));

    midint_mask_extra_bits(a);
}

void midint_shl(struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    auto b_bits = midint_unsigned_sig_bits(b);
    if (b_bits >= log2(a->n_bits))
        MID_CRASH("shift amount too high");

    // the maximum nr of bits is INT32_MAX so the maximum shift amount should
    // be able to fit in a single word
    assert(!is_bignum_used(b_bits));
    midint_shl_imm(a, b->v.words[0]);
}

void midint_shl_imm(struct mid_APInt *a, u64 count)
{
    assert(count < (u64)a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        shl_bignum_case(a, count);
    } else {
        a->v.val <<= count;
        midint_mask_extra_bits(a);
    }
}

void midint_lshr(struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    auto b_bits = midint_unsigned_sig_bits(b);
    if (b_bits >= log2(a->n_bits))
        MID_CRASH("shift amount too high");

    // the maximum nr of bits is INT32_MAX so the maximum shift amount should
    // be able to fit in a single word
    assert(!is_bignum_used(b_bits));
    midint_lshr_imm(a, b->v.words[0]);
}

void midint_lshr_imm(struct mid_APInt *a, u64 count)
{
    assert(count < (u64)a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        lshr_bignum_case(a, count);
    } else {
        a->v.val >>= count;
        midint_mask_extra_bits(a);
    }
}

void midint_ashr(struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    auto b_bits = midint_unsigned_sig_bits(b);
    if (b_bits >= log2(a->n_bits))
        MID_CRASH("shift amount too high");

    // the maximum nr of bits is INT32_MAX so the maximum shift amount should
    // be able to fit in a single word
    assert(!is_bignum_used(b_bits));
    midint_ashr_imm(a, b->v.words[0]);
}

void midint_ashr_imm(struct mid_APInt *a, u64 count)
{
    assert(count < (u64)a->n_bits);

    if (count == 0)
        return;

    if (is_bignum_used(a->n_bits)) {
        ashr_bignum_case(a, count);
    } else {
        auto full_word = sign_ext_word(a->v.val, a->n_bits, midint_word_n_bits);
        a->v.val = shift_arith_right(full_word, count);
        midint_mask_extra_bits(a);
    }
}

struct mid_APInt midint_nip_add(const struct mid_APInt *a,
                                const struct mid_APInt *b)
{
    struct mid_APInt res = midint_copy(a);
    midint_add(&res, b);
    return res;
}

struct mid_APInt midint_nip_add_uimm(const struct mid_APInt *a, u64 b)
{
    struct mid_APInt res = midint_copy(a);
    midint_add_uimm(&res, b);
    return res;
}

struct mid_APInt midint_nip_sub(const struct mid_APInt *a,
                                const struct mid_APInt *b)
{
    struct mid_APInt res = midint_copy(a);
    midint_sub(&res, b);
    return res;
}

struct mid_APInt midint_nip_sub_uimm(const struct mid_APInt *a, u64 b)
{
    struct mid_APInt res = midint_copy(a);
    midint_sub_uimm(&res, b);
    return res;
}

struct mid_APInt midint_nip_shl(const struct mid_APInt *a,
                                const struct mid_APInt *b)
{
    struct mid_APInt res = midint_copy(a);
    midint_shl(&res, b);
    return res;
}

struct mid_APInt midint_nip_shl_imm(const struct mid_APInt *a, u64 b)
{
    struct mid_APInt res = midint_copy(a);
    midint_shl_imm(&res, b);
    return res;
}

struct mid_APInt midint_nip_lshr(const struct mid_APInt *a,
                                 const struct mid_APInt *b)
{
    struct mid_APInt res = midint_copy(a);
    midint_lshr(&res, b);
    return res;
}

struct mid_APInt midint_nip_lshr_imm(const struct mid_APInt *a, u64 b)
{
    struct mid_APInt res = midint_copy(a);
    midint_lshr_imm(&res, b);
    return res;
}

struct mid_APInt midint_nip_ashr(const struct mid_APInt *a,
                                 const struct mid_APInt *b)
{
    struct mid_APInt res = midint_copy(a);
    midint_ashr(&res, b);
    return res;
}

struct mid_APInt midint_nip_ashr_imm(const struct mid_APInt *a, u64 b)
{
    struct mid_APInt res = midint_copy(a);
    midint_ashr_imm(&res, b);
    return res;
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

static void convert_to_u32_digits(const midint_Word *words, i32 n_words,
                                  u32 *digits)
{
    for (i32 i = 0; i < n_words; ++i) {
        digits[i * 2] = words[i];
        digits[i * 2 + 1] = words[i] >> 32;
    }
}

// i would like to say i understand any of this but i really dont
static void bignum_div(const midint_Word *a, i32 a_n_words,
                       const midint_Word *b, i32 b_n_words,
                       midint_Word *out_quot, midint_Word *out_rem)
{
    assert(a_n_words >= b_n_words);

    // assumes midint_Word is 64 bits
    static_assert(sizeof(midint_Word) == sizeof(u64));

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

struct mid_APInt midint_nip_udiv(const struct mid_APInt *a,
                                 const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (!is_bignum_used(a->n_bits)) {
        if (b->v.val == 0)
            MID_CRASH("division by zero");
        return midint_init(a->n_bits, a->v.val / b->v.val, false);
    } else {
        i32 a_words = get_n_words(midint_unsigned_sig_bits(a));
        i32 b_bits = midint_unsigned_sig_bits(b);
        i32 b_words = get_n_words(b_bits);

        // degenerate cases
        if (b_bits == 0)
            MID_CRASH("division by zero");
        if (a_words == 0)
            // 0 / x = 0
            return midint_zero(a->n_bits);
        if (b_bits == 1)
            // x / 1 = x
            return midint_copy(a);
        if (a_words < b_words || midint_is_ult(a, b))
            // x / y = 0 if x < y
            return midint_zero(a->n_bits);
        if (midint_is_eq(a, b))
            // x / x = 1
            return midint_init(a->n_bits, 1, false);
        if (a_words == 1) // b_words must also be 1 in this case cuz it can't
                          // be less
            return midint_init(a->n_bits, a->v.words[0] / b->v.words[0], false);

        struct mid_APInt quot = midint_zero(a->n_bits);
        bignum_div(a->v.words, a_words, b->v.words, b_words, quot.v.words,
                   NULL);
        return quot;
    }
}

struct mid_APInt midint_nip_sdiv(const struct mid_APInt *a,
                                 const struct mid_APInt *b)
{
    bool a_neg = midint_get_sign_bit(a);
    bool b_neg = midint_get_sign_bit(b);

    if (a_neg && b_neg) {
        // -a / -b = +c
        auto pos_a = midint_nip_negate(a);
        auto pos_b = midint_nip_negate(b);
        auto res = midint_nip_udiv(&pos_a, &pos_b);

        mid_APInt_deinit(&pos_a);
        mid_APInt_deinit(&pos_b);
        return res;
    } else if (a_neg) {
        // -a / +b = -c
        auto pos_a = midint_nip_negate(a);
        auto res = midint_nip_udiv(&pos_a, b);
        midint_negate(&res);

        mid_APInt_deinit(&pos_a);
        return res;
    } else if (b_neg) {
        // +a / -b = -c
        auto pos_b = midint_nip_negate(b);
        auto res = midint_nip_udiv(a, &pos_b);
        midint_negate(&res);

        mid_APInt_deinit(&pos_b);
        return res;
    } else {
        // +a / +b = +c
        return midint_nip_udiv(a, b);
    }
}

void midint_udiv(struct mid_APInt *a, const struct mid_APInt *b)
{
    auto tmp = midint_nip_udiv(a, b);
    mid_APInt_deinit(a);
    *a = tmp;
}

void midint_sdiv(struct mid_APInt *a, const struct mid_APInt *b)
{
    auto tmp = midint_nip_sdiv(a, b);
    mid_APInt_deinit(a);
    *a = tmp;
}

void midint_urem(struct mid_APInt *a, const struct mid_APInt *b)
{
    auto tmp = midint_nip_urem(a, b);
    mid_APInt_deinit(a);
    *a = tmp;
}

void midint_srem(struct mid_APInt *a, const struct mid_APInt *b)
{
    auto tmp = midint_nip_srem(a, b);
    mid_APInt_deinit(a);
    *a = tmp;
}

struct mid_APInt midint_nip_urem(const struct mid_APInt *a,
                                 const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (!is_bignum_used(a->n_bits)) {
        if (b->v.val == 0)
            MID_CRASH("remainder by zero");
        return midint_init(a->n_bits, a->v.val % b->v.val, false);
    } else {
        i32 a_words = get_n_words(midint_unsigned_sig_bits(a));
        i32 b_bits = midint_unsigned_sig_bits(b);
        i32 b_words = get_n_words(b_bits);

        // degenerate cases
        if (b_bits == 0)
            MID_CRASH("remainder by zero");
        if (a_words == 0)
            // 0 % x = 0
            return midint_zero(a->n_bits);
        if (b_bits == 1)
            // x % 1 = 0
            return midint_zero(a->n_bits);
        if (a_words < b_words || midint_is_ult(a, b))
            // x % y = x if x < y
            return midint_copy(a);
        if (midint_is_eq(a, b))
            // x % x = 0
            return midint_zero(a->n_bits);
        if (a_words == 1) // b_words must also be 1 in this case cuz it can't
                          // be greater than a_words
            return midint_init(a->n_bits, a->v.words[0] % b->v.words[0], false);
        if (midint_is_pow2(b)) {
            // x % 2^w == x & (2^w - 1)
            // TODO: implement this optimization

            /*
            struct mid_APInt rem = midint_copy(a);
            midint_clear_bits(&rem, );
            */
        }

        struct mid_APInt rem = midint_zero(a->n_bits);
        bignum_div(a->v.words, a_words, b->v.words, b_words, NULL, rem.v.words);
        return rem;
    }
}

struct mid_APInt midint_nip_srem(const struct mid_APInt *a,
                                 const struct mid_APInt *b)
{
    bool a_neg = midint_get_sign_bit(a);
    bool b_neg = midint_get_sign_bit(b);

    if (a_neg && b_neg) {
        // -a % -b = -c
        auto pos_a = midint_nip_negate(a);
        auto pos_b = midint_nip_negate(b);
        auto res = midint_nip_urem(&pos_a, &pos_b);
        midint_negate(&res);

        mid_APInt_deinit(&pos_a);
        mid_APInt_deinit(&pos_b);
        return res;
    } else if (a_neg) {
        // -a % +b = -c
        auto pos_a = midint_nip_negate(a);
        auto res = midint_nip_urem(&pos_a, b);
        midint_negate(&res);

        mid_APInt_deinit(&pos_a);
        return res;
    } else if (b_neg) {
        // +a % -b = +c
        auto pos_b = midint_nip_negate(b);
        auto res = midint_nip_urem(a, &pos_b);

        mid_APInt_deinit(&pos_b);
        return res;
    } else {
        // +a % +b = +c
        return midint_nip_urem(a, b);
    }
}

// compare two unsigned bignums
static int cmp_bignums(const midint_Word *a, const midint_Word *b, i32 a_words,
                       i32 b_words)
{
    while (a_words--, b_words--) {
        if (a_words > 0 && b_words > 0) {
            if (a[a_words] != b[b_words])
                return (a[a_words] > b[b_words]) ? 1 : -1;
        } else if (a_words > 0) {
            if (a[a_words] != 0)
                return 1;
        } else if (b_words > 0) {
            if (b[b_words] != 0)
                return -1;
        }
    }

    return 0;
}

// a and b can be different widths
static int unsigned_cmp_diff_sizes(const struct mid_APInt *a,
                                   const struct mid_APInt *b)
{
    if (is_bignum_used(a->n_bits) && is_bignum_used(b->n_bits)) {
        return cmp_bignums(a->v.words, b->v.words, get_n_words(a->n_bits),
                           get_n_words(b->n_bits));
    } else if (is_bignum_used(a->n_bits)) {
        if (midint_unsigned_sig_bits(a) > midint_word_n_bits)
            return 1;
        else
            return a->v.words[0] < b->v.val ? -1 : a->v.words[0] > b->v.val;
    } else if (is_bignum_used(b->n_bits)) {
        return -unsigned_cmp_diff_sizes(b, a);
    } else {
        return a->v.val < b->v.val ? -1 : a->v.val > b->v.val;
    }
}

void midint_udivrem(const struct mid_APInt *a, const struct mid_APInt *b,
                    struct mid_APInt *out_quot, struct mid_APInt *out_rem)
{
    assert(out_quot && out_rem);
    assert(out_quot->n_bits >= a->n_bits);
    assert(out_rem->n_bits >= b->n_bits);

    if (!is_bignum_used(a->n_bits)) {
        if (b->v.val == 0)
            MID_CRASH("division by 0");

        auto q_val = a->v.val / b->v.val;
        auto r_val = a->v.val % b->v.val;
        out_quot->v.val = q_val;
        out_rem->v.val = r_val;
        return;
    }

    i32 a_words = get_n_words(midint_unsigned_sig_bits(a));
    i32 b_bits = midint_unsigned_sig_bits(b);
    i32 b_words = get_n_words(b_bits);

    // degenerate cases
    if (a_words == 0) {
        midint_assign_uimm(out_quot, 0); // 0 / x = 0
        midint_assign_uimm(out_rem, 0);  // 0 % x = 0
        return;
    }
    if (b_bits == 1) {
        midint_assign(out_quot, a);     // x / 1 = x
        midint_assign_uimm(out_rem, 0); // x % 1 = 0
        return;
    }
    if (a_words < b_words || unsigned_cmp_diff_sizes(a, b) < 0) {
        midint_assign_uimm(out_quot, 0); // x / y = 0 if x < y
        midint_assign(out_rem, a);       // x % y = x if x < y
        return;
    }
    if (unsigned_cmp_diff_sizes(a, b) == 0) {
        midint_assign_uimm(out_quot, 1); // x / x = 1
        midint_assign_uimm(out_rem, 0);  // x % x = 0
        return;
    }

    const midint_Word *a_arr =
        is_bignum_used(a->n_bits) ? a->v.words : &a->v.val;
    const midint_Word *b_arr =
        is_bignum_used(b->n_bits) ? b->v.words : &b->v.val;

    bignum_div(a_arr, a_words, b_arr, b_words, out_quot->v.words,
               out_rem->v.words);
}

void midint_sdivrem(const struct mid_APInt *a, const struct mid_APInt *b,
                    struct mid_APInt *out_quot, struct mid_APInt *out_rem)
{
    assert(out_quot && out_rem);
    assert(out_quot->n_bits >= a->n_bits);
    assert(out_rem->n_bits >= b->n_bits);

    bool a_neg = midint_get_sign_bit(a);
    bool b_neg = midint_get_sign_bit(b);

    if (a_neg && b_neg) {
        // -a / -b = +c
        // -a % -b = -c
        auto pos_a = midint_nip_negate(a);
        auto pos_b = midint_nip_negate(b);
        midint_udivrem(&pos_a, &pos_b, out_quot, out_rem);
        midint_negate(out_rem);

        mid_APInt_deinit(&pos_a);
        mid_APInt_deinit(&pos_b);
    } else if (a_neg) {
        // -a / +b = -c
        // -a % +b = -c
        auto pos_a = midint_nip_negate(a);
        midint_udivrem(&pos_a, b, out_quot, out_rem);
        midint_negate(out_quot);
        midint_negate(out_rem);

        mid_APInt_deinit(&pos_a);
    } else if (b_neg) {
        // +a / -b = -c
        // +a % -b = +c
        auto pos_b = midint_nip_negate(b);
        midint_udivrem(a, &pos_b, out_quot, out_rem);
        midint_negate(out_quot);

        mid_APInt_deinit(&pos_b);
    } else {
        // +a / +b = +c
        // +a % +b = +c
        midint_udivrem(a, b, out_quot, out_rem);
    }
}

void midint_not(struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        for (i32 i = 0; i < get_n_words(self->n_bits); ++i)
            self->v.words[i] = ~self->v.words[i];
    } else {
        self->v.val = ~self->v.val;
    }

    midint_mask_extra_bits(self);
}

struct mid_APInt midint_nip_not(const struct mid_APInt *self)
{
    auto res = midint_copy(self);
    midint_not(&res);
    return res;
}

void midint_negate(struct mid_APInt *self)
{
    midint_not(self);
    midint_add_uimm(self, 1);
}

struct mid_APInt midint_nip_negate(const struct mid_APInt *self)
{
    auto res = midint_copy(self);
    midint_negate(&res);
    return res;
}

static bool is_word_pow2(midint_Word word)
{
    if (word == 0)
        return false;
    return (word & (word - 1)) == 0;
}

bool midint_is_pow2(const struct mid_APInt *self)
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

bool midint_is_eq(const struct mid_APInt *a, const struct mid_APInt *b)
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

bool midint_is_eq_uimm(const struct mid_APInt *a, u64 b)
{
    return midint_unsigned_cmp_imm(a, b) == 0;
}

bool midint_is_eq_simm(const struct mid_APInt *a, i64 b)
{
    return midint_signed_cmp_imm(a, b) == 0;
}

bool midint_is_ugt(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_unsigned_cmp(a, b) > 0;
}

bool midint_is_ugt_imm(const struct mid_APInt *a, u64 b)
{
    return midint_unsigned_cmp_imm(a, b) > 0;
}

bool midint_is_ugteq(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_unsigned_cmp(a, b) >= 0;
}

bool midint_is_ugteq_imm(const struct mid_APInt *a, u64 b)
{
    return midint_unsigned_cmp_imm(a, b) >= 0;
}

bool midint_is_ult(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_unsigned_cmp(a, b) < 0;
}

bool midint_is_ult_imm(const struct mid_APInt *a, u64 b)
{
    return midint_unsigned_cmp_imm(a, b) < 0;
}

bool midint_is_ulteq(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_unsigned_cmp(a, b) <= 0;
}

bool midint_is_ulteq_imm(const struct mid_APInt *a, u64 b)
{
    return midint_unsigned_cmp_imm(a, b) <= 0;
}

int midint_unsigned_cmp(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        i32 n_words = get_n_words(a->n_bits);
        return cmp_bignums(a->v.words, b->v.words, n_words, n_words);
    } else {
        return a->v.val < b->v.val ? -1 : a->v.val > b->v.val;
    }
}

int midint_unsigned_cmp_imm(const struct mid_APInt *a, u64 b)
{
    // doesn't work otherwise
    static_assert(sizeof(midint_Word) == sizeof(u64));

    auto n_bits = midint_unsigned_sig_bits(a);
    if (n_bits > 64)
        return 1;

    if (is_bignum_used(a->n_bits)) {
        return a->v.words[0] < b ? -1 : a->v.words[0] > b;
    } else {
        return a->v.val < b ? -1 : a->v.val > b;
    }
}

int midint_signed_cmp(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        bool a_neg = midint_is_negative(a);
        bool b_neg = midint_is_negative(b);

        if (a_neg != b_neg)
            return a_neg ? -1 : 1;

        // even negative numbers compare correctly if they both have the same
        // signedness
        i32 n_words = get_n_words(a->n_bits);
        return cmp_bignums(a->v.words, b->v.words, n_words, n_words);
    } else {
        auto a_ext = sign_ext_word(a->v.val, a->n_bits, midint_word_n_bits);
        auto b_ext = sign_ext_word(b->v.val, b->n_bits, midint_word_n_bits);
        return a_ext < b_ext ? -1 : a_ext > b_ext;
    }
}

int midint_signed_cmp_imm(const struct mid_APInt *a, i64 b)
{
    // doesn't work otherwise
    static_assert(sizeof(midint_Word) == sizeof(u64));

    auto n_bits = midint_signed_sig_bits(a);
    if (n_bits > 64)
        return midint_is_negative(a) ? -1 : 1;

    if (is_bignum_used(a->n_bits)) {
        return (i64)a->v.words[0] < b ? -1 : (i64)a->v.words[0] > b;
    } else {
        i64 full = sign_ext_word(a->v.val, a->n_bits, midint_word_n_bits);
        return full < b ? -1 : full > b;
    }
}

bool midint_is_sgt(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_signed_cmp(a, b) > 0;
}

bool midint_is_sgt_imm(const struct mid_APInt *a, i64 b)
{
    return midint_signed_cmp_imm(a, b) > 0;
}

bool midint_is_sgteq(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_signed_cmp(a, b) >= 0;
}

bool midint_is_sgteq_imm(const struct mid_APInt *a, i64 b)
{
    return midint_signed_cmp_imm(a, b) >= 0;
}

bool midint_is_slt(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_signed_cmp(a, b) < 0;
}

bool midint_is_slt_imm(const struct mid_APInt *a, i64 b)
{
    return midint_signed_cmp_imm(a, b) < 0;
}

bool midint_is_slteq(const struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    return midint_signed_cmp(a, b) <= 0;
}

bool midint_is_slteq_imm(const struct mid_APInt *a, i64 b)
{
    return midint_signed_cmp_imm(a, b) <= 0;
}

static midint_Word clear_word_bits(midint_Word word, int lo, int hi)
{
    assert(lo >= 0 && lo < midint_word_n_bits);
    assert(hi > 0 && hi <= midint_word_n_bits);
    assert(hi > lo);

    int n = hi - lo;
    if (n == midint_word_n_bits)
        return 0;

    midint_Word mask = ((1 << n) - 1) << lo;
    return word & mask;
}

void midint_clear_bits(struct mid_APInt *self, i32 lo, i32 hi)
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

        i32 start_base = start_word * midint_word_n_bits;
        i32 end_base = end_word * midint_word_n_bits;
        // base of the word after start_base
        i32 start_next = start_base + midint_word_n_bits;

        i32 start_lo = lo - start_base;
        i32 start_hi = hi >= start_next ? midint_word_n_bits : hi - start_base;

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

void midint_and(struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = 0; i < get_n_words(a->n_bits); ++i)
            a->v.words[i] &= b->v.words[i];
    } else {
        a->v.val &= b->v.val;
    }
}

void midint_or(struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = 0; i < get_n_words(a->n_bits); ++i)
            a->v.words[i] |= b->v.words[i];
    } else {
        a->v.val |= b->v.val;
    }
}

void midint_xor(struct mid_APInt *a, const struct mid_APInt *b)
{
    assert(a->n_bits == b->n_bits);

    if (is_bignum_used(a->n_bits)) {
        for (i32 i = 0; i < get_n_words(a->n_bits); ++i)
            a->v.words[i] ^= b->v.words[i];
    } else {
        a->v.val ^= b->v.val;
    }
}

struct mid_APInt midint_nip_and(const struct mid_APInt *a,
                                const struct mid_APInt *b)
{
    auto res = midint_copy(a);
    midint_and(&res, b);
    return res;
}

struct mid_APInt midint_nip_or(const struct mid_APInt *a,
                               const struct mid_APInt *b)
{
    auto res = midint_copy(a);
    midint_or(&res, b);
    return res;
}

struct mid_APInt midint_nip_xor(const struct mid_APInt *a,
                                const struct mid_APInt *b)
{
    auto res = midint_copy(a);
    midint_xor(&res, b);
    return res;
}

u64 midint_to_uint(const struct mid_APInt *self)
{
    // make sure the number actually fits
    assert(midint_unsigned_sig_bits(self) <= midint_word_n_bits);
    if (is_bignum_used(self->n_bits))
        return self->v.words[0];
    else
        return self->v.val;
}

i64 midint_to_sint(const struct mid_APInt *self)
{
    // make sure the number actually fits
    assert(midint_signed_sig_bits(self) <= midint_word_n_bits);

    if (is_bignum_used(self->n_bits))
        return self->v.words[0];
    else
        // some bits may be masked so make sure to sign extend
        return sign_ext_word(self->v.val, self->n_bits, midint_word_n_bits);
}

bool midint_is_negative(const struct mid_APInt *self)
{
    return midint_get_sign_bit(self);
}

static int count_trailing_zeroes(midint_Word word, int n_bits)
{
    if (word == 0)
        return n_bits;

    int n = 0;
    while (((word >>= 1) & 1) == 0)
        ++n;

    return n;
}

i32 midint_count_trailing_zeroes(const struct mid_APInt *self)
{
    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);

        i32 n = 0;
        for (i32 i = 0; i < n_words; ++i) {
            i32 bits = i == n_words - 1 ? n_bits_in_last_word(self->n_bits)
                                        : midint_word_n_bits;

            if (self->v.words[i] != 0) {
                n += count_trailing_zeroes(self->v.words[i], bits);
                break;
            }

            n += bits;
        }

        return n;
    } else {
        return count_trailing_zeroes(self->v.val, self->n_bits);
    }
}

void midint_inc_bit(struct mid_APInt *self, i32 bit)
{
    assert(bit >= 0 && bit < self->n_bits);

    if (is_bignum_used(self->n_bits)) {
        i32 n_words = get_n_words(self->n_bits);
        i32 start_word = get_n_words(bit + 1);

        for (i32 i = start_word; i < n_words; ++i) {
            auto word = &self->v.words[i];

            i32 start_bit = bit - i * midint_word_n_bits;
            for (i32 j = start_bit; j < midint_word_n_bits; ++j) {
                // flip the bit
                midint_Word mask = 1ULL << j;
                *word ^= mask;

                // if the bit was high then we need to carry over to the next
                // bit if it was low then there's no more carry and the rest of
                // the bits are unchanged
                if ((*word >> j) & 1)
                    goto loop_end;
            }
        }

    loop_end:
    } else {
        self->v.val += 1ULL << bit;
    }

    midint_mask_extra_bits(self);
}

void midint_flip_bit(struct mid_APInt *self, i32 bit)
{
    assert(bit >= 0 && bit < self->n_bits);

    if (is_bignum_used(self->n_bits)) {
        i32 word = bit / midint_word_n_bits;
        i32 word_bit = bit % midint_word_n_bits;
        midint_Word mask = 1ULL << word_bit;
        self->v.words[word] ^= mask;
    } else {
        midint_Word mask = 1ULL << bit;
        self->v.val ^= mask;
    }
}

struct mid_APInt midint_get_umax(const struct mid_APInt *self)
{
    auto ret = midint_init(self->n_bits, -1, true);
    return ret;
}

struct mid_APInt midint_get_smax(const struct mid_APInt *self)
{
    if (self->n_bits == 1)
        return midint_zero(self->n_bits);

    auto ret = midint_one(self->n_bits);
    midint_shl_imm(&ret, self->n_bits - 1);
    midint_sub_uimm(&ret, 1);
    return ret;
}

struct mid_APInt midint_get_smin(const struct mid_APInt *self)
{
    auto ret = midint_one(self->n_bits);
    midint_shl_imm(&ret, self->n_bits - 1);
    return ret;
}
