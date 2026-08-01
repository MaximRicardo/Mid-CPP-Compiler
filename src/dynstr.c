#include "dynstr.h"
#include "mid_alloc.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void grow_to_fit(struct mid_Dynstr *self, u32 min_cap)
{
    if (self->cap < min_cap) {
        self->cap = min_cap;
        self->str = mid_realloc(self->str, self->cap * sizeof(*self->str));
    }
}

/* make sure self->cap is big enough to fit src appended to self->str */
static void grow_to_fit_new_str(struct mid_Dynstr *self, const char *src)
{
    grow_to_fit(self, self->len + strlen(src) + 1);
}

struct mid_Dynstr midstr_init(void)
{
    struct mid_Dynstr str;
    str.len = 0;
    str.cap = midstr_start_cap;
    str.str = mid_malloc(str.cap * sizeof(*str.str));
    str.str[0] = '\0';
    return str;
}

void midstr_deinit(struct mid_Dynstr *self)
{
    free(self->str);
    self->str = NULL;
    self->cap = 0;
    self->len = 0;
}

void midstr_append(struct mid_Dynstr *self, const char *src)
{
    grow_to_fit_new_str(self, src);
    // add a null terminator in case the str was previously NULL
    self->str[self->len] = '\0';

    strcat(self->str, src);
    self->len += strlen(src);
}

void midstr_append_dyn(struct mid_Dynstr *self, const struct mid_Dynstr *other)
{
    grow_to_fit_new_str(self, other->str);
    strcat(self->str, other->str);
    self->len += other->len;
}

void midstr_append_printf(struct mid_Dynstr *self, const char *fmt, ...)
{
    char *new_str = NULL;

    va_list args;
    va_start(args, fmt);
    va_list argscpy;
    va_start(argscpy, fmt);

    int new_len = vsnprintf(new_str, 0, fmt, args);
    new_str = mid_malloc((new_len + 1) * sizeof(*new_str));
    vsprintf(new_str, fmt, argscpy);

    midstr_append(self, new_str);

    free(new_str);

    va_end(argscpy);
    va_end(args);
}

void midstr_shrink_to_fit(struct mid_Dynstr *self)
{
    self->str = mid_realloc(self->str, (self->len + 1) * sizeof(*self->str));
}

void midstr_pop(struct mid_Dynstr *self)
{
    if (self->len == 0)
        return;

    --self->len;
    self->str[self->len] = '\0';
}

void midstr_append_char(struct mid_Dynstr *self, char c)
{
    grow_to_fit(self, self->len + 2);
    self->str[self->len++] = c;
    self->str[self->len] = '\0';
}
