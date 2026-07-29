#pragma once

#include "ints.h"
#include <stdio.h>

typedef u64 APInt_Word;
constexpr u64 APInt_Word_max = UINT64_MAX;
constexpr int APInt_Word_n_bits = 64;
#define APINT_WORD_UNSIGNED_FORMAT PRIu64
#define APINT_WORD_SIGNED_FORMAT PRId64
#define APINT_WORD_HEX_FORMAT PRIx64
#define APINT_WORD_FULL_HEX_FORMAT "016" PRIx64

// an arbitrary width 2's complement integer
struct APInt {
    union {
        APInt_Word val;    // stores values <= APInt_Word_max
        APInt_Word *words; // stores values > APInt_Word_max
    } v;

    i32 n_bits; // starts at 1
};

void APInt_deinit(struct APInt *self);
struct APInt APInt_init(i32 n_bits, APInt_Word val, bool is_signed);
// doesn't check if val can fit within n_bits
struct APInt APInt_init_no_limit_check(i32 n_bits, APInt_Word val,
                                       bool is_signed);
// n_words can be smaller or larger than n_bits, but any extraneous words will
// be ignored and missing words will be either zero or sign extended to fill the
// remaining bits
struct APInt APInt_init_arr(i32 n_bits, const APInt_Word *words, i32 n_words,
                            bool sign_ext);
struct APInt APInt_zero(i32 n_bits);
struct APInt APInt_copy(const struct APInt *src);
// changes the width of the APInt. the new width can also be smaller than the
// old width
void APInt_ext(struct APInt *self, i32 new_n_bits, bool sign_ext);
// returns the value of the nth bit
bool APInt_get_bit(const struct APInt *self, i32 n);
bool APInt_get_sign_bit(const struct APInt *self);
void APInt_log(const struct APInt *self, FILE *out, bool is_signed);
void APInt_log_hex(const struct APInt *self, FILE *out);

void APInt_add(struct APInt *a, const struct APInt *b);
// logical left shift
void APInt_shl(struct APInt *a, i32 count);
// logical right shift
void APInt_lshr(struct APInt *a, i32 count);
