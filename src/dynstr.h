#pragma once

/* resizable strings */

#include "attribute.h"
#include "ints.h"

constexpr mid_isize dynstr_start_cap = 128;

struct Mid_Dynstr {

    // NULL terminated
    char *str;
    mid_isize len; // doesn't count the null terminator
    mid_isize cap; // counts the null terminator
};

struct Mid_Dynstr MidDynstr_init(void);
void MidDynstr_deinit(struct Mid_Dynstr *self);
void MidDynstr_append(struct Mid_Dynstr *self, const char *src);
void MidDynstr_append_dyn(struct Mid_Dynstr *self,
                          const struct Mid_Dynstr *other);
/* appends the formatted string to self */
void MidDynstr_append_printf(struct Mid_Dynstr *self, const char *fmt, ...)
    MID_ATTRIBUTE((format(printf, 2, 3)));
void MidDynstr_append_char(struct Mid_Dynstr *self, char c);
/* doesn't do anything if self->size == 0 */
void MidDynstr_pop(struct Mid_Dynstr *self);
void MidDynstr_shrink_to_fit(struct Mid_Dynstr *self);
