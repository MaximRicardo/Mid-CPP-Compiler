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
struct MidAPInt {
    union {
        MidAPInt_Word val;    // stores values <= MidAPInt_Word_max
        MidAPInt_Word *words; // stores values > MidAPInt_Word_max
    } v;

    i32 n_bits; // starts at 1
};

void MidAPInt_deinit(struct MidAPInt *self);
struct MidAPInt MidAPInt_init(i32 n_bits, MidAPInt_Word val, bool is_signed);
// doesn't check if val can fit within n_bits
struct MidAPInt MidAPInt_init_no_limit_check(i32 n_bits, MidAPInt_Word val,
                                             bool is_signed);
// n_words can be smaller or larger than n_bits, but any extraneous words will
// be ignored and missing words will be either zero or sign extended to fill the
// remaining bits.
// the sign is evaluated based on the last bit in the last word of words
struct MidAPInt MidAPInt_init_arr(i32 n_bits, const MidAPInt_Word *words,
                                  i32 n_words, bool sign_ext);
struct MidAPInt MidAPInt_zero(i32 n_bits);
struct MidAPInt MidAPInt_copy(const struct MidAPInt *src);
// changes the width of the APInt. the new width can also be smaller than the
// old width
void MidAPInt_ext(struct MidAPInt *self, i32 new_n_bits, bool sign_ext);
// returns the value of the nth bit
bool MidAPInt_get_bit(const struct MidAPInt *self, i32 n);
bool MidAPInt_get_sign_bit(const struct MidAPInt *self);
void MidAPInt_log(const struct MidAPInt *self, FILE *out, bool is_signed);
void MidAPInt_log_hex(const struct MidAPInt *self, FILE *out);
// nr of bits required to represent the unsigned number in self
i32 MidAPInt_unsigned_sig_bits(const struct MidAPInt *self);
// nr of bits required to represent the signed number in self
i32 MidAPInt_signed_sig_bits(const struct MidAPInt *self);
// clears any extra bits that are set past self->n_bits
void MidAPInt_mask_extra_bits(struct MidAPInt *self);
u64 MidAPInt_to_uint(const struct MidAPInt *self);
i64 MidAPInt_to_sint(const struct MidAPInt *self);

// in place operations
void MidAPInt_assign(struct MidAPInt *dest, const struct MidAPInt *src);
void MidAPInt_add(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_add_imm(struct MidAPInt *a, u64 b);
void MidAPInt_sub(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_sub_imm(struct MidAPInt *a, u64 b);
void MidAPInt_mul(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_mul_imm(struct MidAPInt *a, u64 b);
void MidAPInt_udiv(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_sdiv(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_urem(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_srem(struct MidAPInt *a, const struct MidAPInt *b);
// logical left shift
void MidAPInt_shl(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_shl_imm(struct MidAPInt *a, u64 count);
// logical right shift
void MidAPInt_lshr(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_lshr_imm(struct MidAPInt *a, u64 count);
// arithmetic right shift
void MidAPInt_ashr(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_ashr_imm(struct MidAPInt *a, u64 count);
// lo is inclusive, hi is exclusive
void MidAPInt_clear_bits(struct MidAPInt *self, i32 lo, i32 hi);
void MidAPInt_negate(struct MidAPInt *self);
// bitwise operations
void MidAPInt_not(struct MidAPInt *self);
void MidAPInt_and(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_or(struct MidAPInt *a, const struct MidAPInt *b);
void MidAPInt_xor(struct MidAPInt *a, const struct MidAPInt *b);

// not in place operations
struct MidAPInt MidAPInt_nip_add(const struct MidAPInt *a,
                                 const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_add_imm(const struct MidAPInt *a, u64 b);
struct MidAPInt MidAPInt_nip_sub(const struct MidAPInt *a,
                                 const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_sub_imm(const struct MidAPInt *a, u64 b);
struct MidAPInt MidAPInt_nip_mul(const struct MidAPInt *a,
                                 const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_mul_imm(const struct MidAPInt *a, u64 b);
struct MidAPInt MidAPInt_nip_udiv(const struct MidAPInt *a,
                                  const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_sdiv(const struct MidAPInt *a,
                                  const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_urem(const struct MidAPInt *a,
                                  const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_srem(const struct MidAPInt *a,
                                  const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_shl(const struct MidAPInt *a,
                                 const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_shl_imm(const struct MidAPInt *a, u64 b);
struct MidAPInt MidAPInt_nip_lshr(const struct MidAPInt *a,
                                  const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_lshr_imm(const struct MidAPInt *a, u64 b);
struct MidAPInt MidAPInt_nip_ashr(const struct MidAPInt *a,
                                  const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_ashr_imm(const struct MidAPInt *a, u64 b);
struct MidAPInt MidAPInt_nip_negate(const struct MidAPInt *self);
struct MidAPInt MidAPInt_nip_not(const struct MidAPInt *self);
struct MidAPInt MidAPInt_nip_and(const struct MidAPInt *a,
                                 const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_or(const struct MidAPInt *a,
                                const struct MidAPInt *b);
struct MidAPInt MidAPInt_nip_xor(const struct MidAPInt *a,
                                 const struct MidAPInt *b);

/*
 * computes the div and rem at the same time for the cost of only one
 * a and b can also be passed as the outputs of the function
 * BOTH OUTPUTS ARE REQUIRED AND CAN NOT BE NULL!
 */
void MidAPInt_udivrem(const struct MidAPInt *a, const struct MidAPInt *b,
                      struct MidAPInt *out_quot, struct MidAPInt *out_rem);
void MidAPInt_sdivrem(const struct MidAPInt *a, const struct MidAPInt *b,
                      struct MidAPInt *out_quot, struct MidAPInt *out_rem);

// comparisons
bool MidAPInt_is_zero(const struct MidAPInt *self);
bool MidAPInt_is_signed_min(const struct MidAPInt *self);
bool MidAPInt_is_all_ones(const struct MidAPInt *self);
bool MidAPInt_is_negative(const struct MidAPInt *self); // same as get_sign_bit
bool MidAPInt_is_pow2(const struct MidAPInt *self);
bool MidAPInt_is_eq(const struct MidAPInt *a, const struct MidAPInt *b);
// unsigned comparisons
bool MidAPInt_is_ugt(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_ugt_imm(const struct MidAPInt *a, u64 b);
bool MidAPInt_is_ugteq(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_ugteq_imm(const struct MidAPInt *a, u64 b);
bool MidAPInt_is_ult(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_ult_imm(const struct MidAPInt *a, u64 b);
bool MidAPInt_is_ulteq(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_ulteq_imm(const struct MidAPInt *a, u64 b);
// returns -1 if a < b, 1 if a > b, and 0 if a == b
int MidAPInt_unsigned_cmp(const struct MidAPInt *a, const struct MidAPInt *b);
int MidAPInt_unsigned_cmp_imm(const struct MidAPInt *a, u64 b);
// signed comparisons
bool MidAPInt_is_sgt(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_sgt_imm(const struct MidAPInt *a, i64 b);
bool MidAPInt_is_sgteq(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_sgteq_imm(const struct MidAPInt *a, i64 b);
bool MidAPInt_is_slt(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_slt_imm(const struct MidAPInt *a, i64 b);
bool MidAPInt_is_slteq(const struct MidAPInt *a, const struct MidAPInt *b);
bool MidAPInt_is_slteq_imm(const struct MidAPInt *a, i64 b);
// returns -1 if a < b, 1 if a > b, and 0 if a == b
int MidAPInt_signed_cmp(const struct MidAPInt *a, const struct MidAPInt *b);
int MidAPInt_signed_cmp_imm(const struct MidAPInt *a, i64 b);
