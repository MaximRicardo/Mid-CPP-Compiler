#pragma once

#include "ints.h"
#include <stdio.h>

typedef u64 MidAPInt_Word;
constexpr u64 MidAPInt_word_max = UINT64_MAX;
constexpr int MidAPInt_word_n_bits = 64;
#define MIDAPINT_WORD_UNSIGNED_FORMAT PRIu64
#define MIDAPINT_WORD_SIGNED_FORMAT PRId64
#define MIDAPINT_WORD_HEX_FORMAT PRIx64
#define MIDAPINT_WORD_FULL_HEX_FORMAT "016" PRIx64

// an arbitrary width 2's complement integer
struct Mid_APInt {
    union {
        MidAPInt_Word val;    // stores values <= MidAPInt_Word_max
        MidAPInt_Word *words; // stores values > MidAPInt_Word_max
    } v;

    i32 n_bits; // starts at 1
};

void MidAPInt_deinit(struct Mid_APInt *self);
struct Mid_APInt MidAPInt_init(i32 n_bits, MidAPInt_Word val, bool is_signed);
// doesn't check if val can fit within n_bits
struct Mid_APInt MidAPInt_init_no_limit_check(i32 n_bits, MidAPInt_Word val,
                                              bool is_signed);
// n_words can be smaller or larger than n_bits, but any extraneous words will
// be ignored and missing words will be either zero or sign extended to fill the
// remaining bits
struct Mid_APInt MidAPInt_init_arr(i32 n_bits, const MidAPInt_Word *words,
                                   i32 n_words, bool sign_ext);
struct Mid_APInt MidAPInt_zero(i32 n_bits);
struct Mid_APInt MidAPInt_copy(const struct Mid_APInt *src);
void MidAPInt_copy_value(struct Mid_APInt *restrict dest,
                         const struct Mid_APInt *restrict src);
// changes the width of the APInt. the new width can also be smaller than the
// old width
void MidAPInt_ext(struct Mid_APInt *self, i32 new_n_bits, bool sign_ext);
// returns the value of the nth bit
bool MidAPInt_get_bit(const struct Mid_APInt *self, i32 n);
bool MidAPInt_get_sign_bit(const struct Mid_APInt *self);
void MidAPInt_log(const struct Mid_APInt *self, FILE *out, bool is_signed);
void MidAPInt_log_hex(const struct Mid_APInt *self, FILE *out);
// nr of bits required to represent the number in self
i32 MidAPInt_n_active_bits(const struct Mid_APInt *self);

// in place operations
void MidAPInt_add(struct Mid_APInt *a, const struct Mid_APInt *b);
void MidAPInt_udiv(struct Mid_APInt *a, const struct Mid_APInt *b);
void MidAPInt_urem(struct Mid_APInt *a, const struct Mid_APInt *b);
// logical left shift
void MidAPInt_shl(struct Mid_APInt *a, i32 count);
// logical right shift
void MidAPInt_lshr(struct Mid_APInt *a, i32 count);
// lo is inclusive, hi is exclusive
void MidAPInt_clear_bits(struct Mid_APInt *self, i32 lo, i32 hi);

// not in place operations
struct Mid_APInt MidAPInt_nip_udiv(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b);
struct Mid_APInt MidAPInt_nip_urem(const struct Mid_APInt *a,
                                   const struct Mid_APInt *b);

/*
 * computes the div and rem at the same time for the cost of only one
 * a and b can also be passed as the outputs of the function
 * BOTH OUTPUTS ARE REQUIRED AND CAN NOT BE NULL!
 */
void MidAPInt_udivrem(const struct Mid_APInt *a, const struct Mid_APInt *b,
                      struct Mid_APInt *out_quot, struct Mid_APInt *out_rem);

// comparisons
bool MidAPInt_is_zero(const struct Mid_APInt *self);
bool MidAPInt_is_pow2(const struct Mid_APInt *self);
bool MidAPInt_is_eq(const struct Mid_APInt *a, const struct Mid_APInt *b);
// unsigned comparisons
bool MidAPInt_is_ugt(const struct Mid_APInt *a, const struct Mid_APInt *b);
bool MidAPInt_is_ugteq(const struct Mid_APInt *a, const struct Mid_APInt *b);
bool MidAPInt_is_ult(const struct Mid_APInt *a, const struct Mid_APInt *b);
bool MidAPInt_is_ulteq(const struct Mid_APInt *a, const struct Mid_APInt *b);
