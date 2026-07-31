#pragma once

#include "ints.h"
#include <stdio.h>

typedef u64 MidAPInt_Word;
constexpr u64 MidAPInt_Word_max = UINT64_MAX;
constexpr int MidAPInt_Word_n_bits = 64;
#define MIDAPINT_WORD_UNSIGNED_FORMAT PRIu64
#define MIDAPINT_WORD_SIGNED_FORMAT PRId64
#define MIDAPINT_WORD_HEX_FORMAT PRIx64
#define MIDAPINT_WORD_FULL_HEX_FORMAT "016" PRIx64

// an arbitrary width 2's complement integer
struct APInt {
    union {
        MidAPInt_Word val;    // stores values <= MidAPInt_Word_max
        MidAPInt_Word *words; // stores values > MidAPInt_Word_max
    } v;

    i32 n_bits; // starts at 1
};

void MidAPInt_deinit(struct APInt *self);
struct APInt MidAPInt_init(i32 n_bits, MidAPInt_Word val, bool is_signed);
// doesn't check if val can fit within n_bits
struct APInt MidAPInt_init_no_limit_check(i32 n_bits, MidAPInt_Word val,
                                          bool is_signed);
// n_words can be smaller or larger than n_bits, but any extraneous words will
// be ignored and missing words will be either zero or sign extended to fill the
// remaining bits
struct APInt MidAPInt_init_arr(i32 n_bits, const MidAPInt_Word *words,
                               i32 n_words, bool sign_ext);
struct APInt MidAPInt_zero(i32 n_bits);
struct APInt MidAPInt_copy(const struct APInt *src);
void MidAPInt_copy_value(struct APInt *restrict dest,
                         const struct APInt *restrict src);
// changes the width of the APInt. the new width can also be smaller than the
// old width
void MidAPInt_ext(struct APInt *self, i32 new_n_bits, bool sign_ext);
// returns the value of the nth bit
bool MidAPInt_get_bit(const struct APInt *self, i32 n);
bool MidAPInt_get_sign_bit(const struct APInt *self);
void MidAPInt_log(const struct APInt *self, FILE *out, bool is_signed);
void MidAPInt_log_hex(const struct APInt *self, FILE *out);
// nr of bits required to represent the number in self
i32 MidAPInt_n_active_bits(const struct APInt *self);

// in place operations
void MidAPInt_add(struct APInt *a, const struct APInt *b);
void MidAPInt_udiv(struct APInt *a, const struct APInt *b);
void MidAPInt_urem(struct APInt *a, const struct APInt *b);
// logical left shift
void MidAPInt_shl(struct APInt *a, i32 count);
// logical right shift
void MidAPInt_lshr(struct APInt *a, i32 count);
// lo is inclusive, hi is exclusive
void MidAPInt_clear_bits(struct APInt *self, i32 lo, i32 hi);

// not in place operations
struct APInt MidAPInt_nip_udiv(const struct APInt *a, const struct APInt *b);
struct APInt MidAPInt_nip_urem(const struct APInt *a, const struct APInt *b);

/*
 * computes the div and rem at the same time for the cost of only one
 * a and b can also be passed as the outputs of the function
 * BOTH OUTPUTS ARE REQUIRED AND CAN NOT BE NULL!
 */
void MidAPInt_udivrem(const struct APInt *a, const struct APInt *b,
                      struct APInt *out_quot, struct APInt *out_rem);

// comparisons
bool MidAPInt_is_zero(const struct APInt *self);
bool MidAPInt_is_pow2(const struct APInt *self);
bool MidAPInt_is_eq(const struct APInt *a, const struct APInt *b);
// unsigned comparisons
bool MidAPInt_is_ugt(const struct APInt *a, const struct APInt *b);
bool MidAPInt_is_ugteq(const struct APInt *a, const struct APInt *b);
bool MidAPInt_is_ult(const struct APInt *a, const struct APInt *b);
bool MidAPInt_is_ulteq(const struct APInt *a, const struct APInt *b);
