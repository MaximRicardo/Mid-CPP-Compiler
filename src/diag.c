#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
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

struct Diag Diag_expected_token_err(const char *name,
                                    const struct Lexer_Token *tok,
                                    enum ErrorType type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("expected %s", name),
        .err = type,
        .is_err = true,
    };
}

struct Diag Diag_expected_token_warn(const char *name,
                                     const struct Lexer_Token *tok,
                                     enum WarnType type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("expected %s", name),
        .warn = type,
        .is_err = false,
    };
}

struct Diag Diag_unexpected_token_err(const char *name,
                                      const struct Lexer_Token *tok,
                                      enum ErrorType type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("unexpected %s", name),
        .err = type,
        .is_err = true,
    };
}

struct Diag Diag_unexpected_token_warn(const char *name,
                                       const struct Lexer_Token *tok,
                                       enum WarnType type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("unexpected %s", name),
        .warn = type,
        .is_err = false,
    };
}

struct Diag Diag_ident_redefined_err(const char *name,
                                     const struct Lexer_Token *tok,
                                     enum ErrorType type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("'%s' redefined", name),
        .err = type,
        .is_err = true,
    };
}

struct Diag Diag_ident_redefined_warn(const char *name,
                                      const struct Lexer_Token *tok,
                                      enum WarnType type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("'%s' redefined", name),
        .warn = type,
        .is_err = false,
    };
}
