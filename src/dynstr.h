#pragma once

/* resizable strings */

#include "attribute.h"
#include "ints.h"

constexpr isize_t dynstr_start_cap = 128;

struct Dynstr {

    // NULL terminated
    char *str;
    isize_t len; // doesn't count the null terminator
    isize_t cap; // counts the null terminator
};

struct Dynstr Dynstr(void);
void Dynstr_deinit(struct Dynstr *self);
void Dynstr_append(struct Dynstr *self, const char *src);
void Dynstr_append_dyn(struct Dynstr *self, const struct Dynstr *other);
/* appends the formatted string to self */
void Dynstr_append_printf(struct Dynstr *self, const char *fmt, ...)
    ATTRIBUTE((format(printf, 2, 3)));
void Dynstr_append_char(struct Dynstr *self, char c);
/* doesn't do anything if self->size == 0 */
void Dynstr_pop(struct Dynstr *self);
void Dynstr_shrink_to_fit(struct Dynstr *self);
