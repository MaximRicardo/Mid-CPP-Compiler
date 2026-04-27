#include "dynstr.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void grow_to_fit(struct Dynstr *self, u32 min_cap)
{
    if (self->cap < min_cap) {
        self->cap = min_cap;
        self->str = realloc(self->str, self->cap * sizeof(*self->str));
    }
}

/* make sure self->cap is big enough to fit src appended to self->str */
static void grow_to_fit_new_str(struct Dynstr *self, const char *src)
{
    grow_to_fit(self, self->len + strlen(src) + 1);
}

struct Dynstr Dynstr(void)
{
    struct Dynstr str;
    str.len = 0;
    str.cap = dynstr_start_cap;
    str.str = malloc(str.cap * sizeof(*str.str));
    str.str[0] = '\0';
    return str;
}

void Dynstr_deinit(struct Dynstr *self)
{
    free(self->str);
    self->str = NULL;
    self->cap = 0;
    self->len = 0;
}

void Dynstr_append(struct Dynstr *self, const char *src)
{
    grow_to_fit_new_str(self, src);
    strcat(self->str, src);
    self->len += strlen(src);
}

void Dynstr_append_dyn(struct Dynstr *self, const struct Dynstr *other)
{
    grow_to_fit_new_str(self, other->str);
    strcat(self->str, other->str);
    self->len += other->len;
}

void Dynstr_append_printf(struct Dynstr *self, const char *fmt, ...)
{
    char *new_str = NULL;

    va_list args;
    va_start(args, fmt);
    va_list argscpy;
    va_start(argscpy, fmt);

    int new_len = vsnprintf(new_str, 0, fmt, args);
    new_str = malloc((new_len + 1) * sizeof(*new_str));
    vsprintf(new_str, fmt, argscpy);

    Dynstr_append(self, new_str);

    free(new_str);

    va_end(argscpy);
    va_end(args);
}

void Dynstr_shrink_to_fit(struct Dynstr *self)
{
    self->str = realloc(self->str, (self->len + 1) * sizeof(*self->str));
}

void Dynstr_pop(struct Dynstr *self)
{
    if (self->len == 0)
        return;

    --self->len;
    self->str[self->len] = '\0';
}

void Dynstr_append_char(struct Dynstr *self, char c)
{
    grow_to_fit(self, self->len + 2);
    self->str[self->len++] = c;
    self->str[self->len] = '\0';
}
