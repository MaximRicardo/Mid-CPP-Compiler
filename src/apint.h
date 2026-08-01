#pragma once

#include "ints.h"
#include <stdio.h>

typedef u64 midint_Word;
constexpr u64 midint_word_max = UINT64_MAX;
constexpr int midint_word_n_bits = 64;
#define MIDINT_WORD_UNSIGNED_FORMAT PRIu64
#define MIDINT_WORD_SIGNED_FORMAT PRId64
#define MIDINT_WORD_HEX_FORMAT PRIx64
#define MIDINT_WORD_FULL_HEX_FORMAT "016" PRIx64

// an arbitrary width 2's complement integer
struct mid_APInt {
    union {
        midint_Word val;    // stores values <= midint_Word_max
        midint_Word *words; // stores values > midint_Word_max
    } v;

    i32 n_bits; // starts at 1
};

void midint_deinit(struct mid_APInt *self);
struct mid_APInt midint_init(i32 n_bits, midint_Word val, bool is_signed);
// doesn't check if val can fit within n_bits
struct mid_APInt midint_init_no_limit_check(i32 n_bits, midint_Word val,
                                            bool is_signed);
// n_words can be smaller or larger than n_bits, but any extraneous words will
// be ignored and missing words will be either zero or sign extended to fill the
// remaining bits.
// the sign is evaluated based on the last bit in the last word of words
struct mid_APInt midint_init_arr(i32 n_bits, const midint_Word *words,
                                 i32 n_words, bool sign_ext);
struct mid_APInt midint_alloc(i32 n_bits);
struct mid_APInt midint_zero(i32 n_bits);
struct mid_APInt midint_copy(const struct mid_APInt *src);
// changes the width of the APInt. the new width can also be smaller than the
// old width
void midint_ext(struct mid_APInt *self, i32 new_n_bits, bool sign_ext);
// returns the value of the nth bit
bool midint_get_bit(const struct mid_APInt *self, i32 n);
bool midint_get_sign_bit(const struct mid_APInt *self);
void midint_log(const struct mid_APInt *self, FILE *out, bool is_signed);
void midint_log_hex(const struct mid_APInt *self, FILE *out);
// nr of bits required to represent the unsigned number in self
i32 midint_unsigned_sig_bits(const struct mid_APInt *self);
// nr of bits required to represent the signed number in self
i32 midint_signed_sig_bits(const struct mid_APInt *self);
// clears any extra bits that are set past self->n_bits
void midint_mask_extra_bits(struct mid_APInt *self);
u64 midint_to_uint(const struct mid_APInt *self);
i64 midint_to_sint(const struct mid_APInt *self);

// in place operations
void midint_assign(struct mid_APInt *dest, const struct mid_APInt *src);
void midint_assign_uimm(struct mid_APInt *dest, u64 src);
void midint_assign_simm(struct mid_APInt *dest, i64 src);
void midint_add(struct mid_APInt *a, const struct mid_APInt *b);
void midint_add_uimm(struct mid_APInt *a, u64 b);
void midint_sub(struct mid_APInt *a, const struct mid_APInt *b);
void midint_sub_uimm(struct mid_APInt *a, u64 b);
void midint_mul(struct mid_APInt *a, const struct mid_APInt *b);
void midint_mul_uimm(struct mid_APInt *a, u64 b);
void midint_udiv(struct mid_APInt *a, const struct mid_APInt *b);
void midint_sdiv(struct mid_APInt *a, const struct mid_APInt *b);
void midint_urem(struct mid_APInt *a, const struct mid_APInt *b);
void midint_srem(struct mid_APInt *a, const struct mid_APInt *b);
// logical left shift
void midint_shl(struct mid_APInt *a, const struct mid_APInt *b);
void midint_shl_imm(struct mid_APInt *a, u64 count);
// logical right shift
void midint_lshr(struct mid_APInt *a, const struct mid_APInt *b);
void midint_lshr_imm(struct mid_APInt *a, u64 count);
// arithmetic right shift
void midint_ashr(struct mid_APInt *a, const struct mid_APInt *b);
void midint_ashr_imm(struct mid_APInt *a, u64 count);
// lo is inclusive, hi is exclusive
void midint_clear_bits(struct mid_APInt *self, i32 lo, i32 hi);
void midint_negate(struct mid_APInt *self);
// bitwise operations
void midint_not(struct mid_APInt *self);
void midint_and(struct mid_APInt *a, const struct mid_APInt *b);
void midint_or(struct mid_APInt *a, const struct mid_APInt *b);
void midint_xor(struct mid_APInt *a, const struct mid_APInt *b);

// not in place operations
struct mid_APInt midint_nip_add(const struct mid_APInt *a,
                                const struct mid_APInt *b);
struct mid_APInt midint_nip_add_uimm(const struct mid_APInt *a, u64 b);
struct mid_APInt midint_nip_sub(const struct mid_APInt *a,
                                const struct mid_APInt *b);
struct mid_APInt midint_nip_sub_uimm(const struct mid_APInt *a, u64 b);
struct mid_APInt midint_nip_mul(const struct mid_APInt *a,
                                const struct mid_APInt *b);
struct mid_APInt midint_nip_mul_uimm(const struct mid_APInt *a, u64 b);
struct mid_APInt midint_nip_udiv(const struct mid_APInt *a,
                                 const struct mid_APInt *b);
struct mid_APInt midint_nip_sdiv(const struct mid_APInt *a,
                                 const struct mid_APInt *b);
struct mid_APInt midint_nip_urem(const struct mid_APInt *a,
                                 const struct mid_APInt *b);
struct mid_APInt midint_nip_srem(const struct mid_APInt *a,
                                 const struct mid_APInt *b);
struct mid_APInt midint_nip_shl(const struct mid_APInt *a,
                                const struct mid_APInt *b);
struct mid_APInt midint_nip_shl_imm(const struct mid_APInt *a, u64 b);
struct mid_APInt midint_nip_lshr(const struct mid_APInt *a,
                                 const struct mid_APInt *b);
struct mid_APInt midint_nip_lshr_imm(const struct mid_APInt *a, u64 b);
struct mid_APInt midint_nip_ashr(const struct mid_APInt *a,
                                 const struct mid_APInt *b);
struct mid_APInt midint_nip_ashr_imm(const struct mid_APInt *a, u64 b);
struct mid_APInt midint_nip_negate(const struct mid_APInt *self);
struct mid_APInt midint_nip_not(const struct mid_APInt *self);
struct mid_APInt midint_nip_and(const struct mid_APInt *a,
                                const struct mid_APInt *b);
struct mid_APInt midint_nip_or(const struct mid_APInt *a,
                               const struct mid_APInt *b);
struct mid_APInt midint_nip_xor(const struct mid_APInt *a,
                                const struct mid_APInt *b);

/*
 * computes the div and rem at the same time for the cost of only one
 * a and b can also be passed as the outputs of the function
 * NOTE: both outputs are required and can not be NULL
 * NOTE: assumes out_quot and out_rem are already allocated
 */
void midint_udivrem(const struct mid_APInt *a, const struct mid_APInt *b,
                    struct mid_APInt *out_quot, struct mid_APInt *out_rem);
void midint_sdivrem(const struct mid_APInt *a, const struct mid_APInt *b,
                    struct mid_APInt *out_quot, struct mid_APInt *out_rem);
/*
 * out_res should be twice as wide as the inputs
 * NOTE: out_res can not be NULL
 * NOTE: assumes out_res is already allocated
 */
void midint_ufullmul(const struct mid_APInt *a, const struct mid_APInt *b,
                     struct mid_APInt *out_res);

// comparisons
bool midint_is_zero(const struct mid_APInt *self);
bool midint_is_signed_min(const struct mid_APInt *self);
bool midint_is_all_ones(const struct mid_APInt *self);
bool midint_is_negative(const struct mid_APInt *self); // same as get_sign_bit
bool midint_is_pow2(const struct mid_APInt *self);
bool midint_is_eq(const struct mid_APInt *a, const struct mid_APInt *b);
// unsigned comparisons
bool midint_is_ugt(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_ugt_imm(const struct mid_APInt *a, u64 b);
bool midint_is_ugteq(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_ugteq_imm(const struct mid_APInt *a, u64 b);
bool midint_is_ult(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_ult_imm(const struct mid_APInt *a, u64 b);
bool midint_is_ulteq(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_ulteq_imm(const struct mid_APInt *a, u64 b);
// returns -1 if a < b, 1 if a > b, and 0 if a == b
int midint_unsigned_cmp(const struct mid_APInt *a, const struct mid_APInt *b);
int midint_unsigned_cmp_imm(const struct mid_APInt *a, u64 b);
// signed comparisons
bool midint_is_sgt(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_sgt_imm(const struct mid_APInt *a, i64 b);
bool midint_is_sgteq(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_sgteq_imm(const struct mid_APInt *a, i64 b);
bool midint_is_slt(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_slt_imm(const struct mid_APInt *a, i64 b);
bool midint_is_slteq(const struct mid_APInt *a, const struct mid_APInt *b);
bool midint_is_slteq_imm(const struct mid_APInt *a, i64 b);
// returns -1 if a < b, 1 if a > b, and 0 if a == b
int midint_signed_cmp(const struct mid_APInt *a, const struct mid_APInt *b);
int midint_signed_cmp_imm(const struct mid_APInt *a, i64 b);
