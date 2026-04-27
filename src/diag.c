#include "diag.h"
#include "print.h"
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
    //          "10 | printf("hello world")"
    printf("%s:%" PRId32 ":%" PRId32 ": %s: %s\n", diag->pos.file,
           diag->pos.line, diag->pos.column, diag->is_err ? "error" : "warning",
           diag->msg);
    printf("%" PRId32 " | ", diag->pos.line);
    Print_line(diag->line);
    putchar('\n');
}
