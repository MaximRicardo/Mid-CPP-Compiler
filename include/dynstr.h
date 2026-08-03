#pragma once

/* resizable strings */

#include "attribute.h"
#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

constexpr mid_isize midstr_start_cap = 128;

struct mid_Dynstr {

    // NULL terminated
    char *str;
    mid_isize len; // doesn't count the null terminator
    mid_isize cap; // counts the null terminator
};

struct mid_Dynstr midstr_init(void);
void midstr_deinit(struct mid_Dynstr *self);
void midstr_append(struct mid_Dynstr *self, const char *src);
void midstr_append_dyn(struct mid_Dynstr *self, const struct mid_Dynstr *other);
/* appends the formatted string to self */
void midstr_append_printf(struct mid_Dynstr *self, const char *fmt, ...)
    MID_ATTRIBUTE((format(printf, 2, 3)));
void midstr_append_char(struct mid_Dynstr *self, char c);
/* doesn't do anything if self->size == 0 */
void midstr_pop(struct mid_Dynstr *self);
void midstr_shrink_to_fit(struct mid_Dynstr *self);

#ifdef __cplusplus
}
#endif
