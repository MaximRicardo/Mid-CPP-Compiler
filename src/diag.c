#include "diag.h"
#include "ints.h"
#include "macros.h"
#include "print.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>

void Diag_deinit(struct Diag *self)
{
    free(self->msg);
    self->msg = NULL;
}

void Diag_print(const struct Diag *diag)
{
    // example: "test.cpp:10:5: error: expected ';' after expression"
    //          "00010 | printf("hello world")"
    //                   ^
    printf("%s:%" PRId32 ":%" PRId32 ": %s: %s\n", diag->pos.file,
           diag->pos.line, diag->pos.column, diag->is_err ? "error" : "warning",
           diag->msg);

    printf("%05" PRId32 " | ", diag->pos.line);
    Print_line(diag->line);
    putchar('\n');

    i32 n_digits = MAX(log10(diag->pos.line) + 1, 5);
    for (isize_t i = 0; i < n_digits; ++i)
        putchar(' ');
    printf(" | ");
    Print_column_arrow(diag->pos.column);
}
