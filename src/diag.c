#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "print.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>

void MidDiag_deinit(struct MidDiag_Diag *self)
{
    free(self->msg);
    self->msg = NULL;
}

void MidDiag_print(const struct MidDiag_Diag *diag)
{
    // example: "test.cpp:10:5: error: expected ';' after expression"
    //          "00010 | printf("hello world")"
    //                   ^
    printf("%s:%" PRId32 ":%" PRId32 ": ", diag->pos.file, diag->pos.line,
           diag->pos.column);
    switch (diag->type) {
    case MIDDIAG_TYPE_ERROR:
        printf("%serror%s: ", MidPrint_ansi_red, MidPrint_ansi_reset);
        break;

    case MIDDIAG_TYPE_WARNING:
        printf("%swarning%s: ", MidPrint_ansi_magenta, MidPrint_ansi_reset);
        break;

    case MIDDIAG_TYPE_NOTE:
        printf("%snote%s: ", MidPrint_ansi_cyan, MidPrint_ansi_reset);
        break;
    }
    printf("%s\n", diag->msg);

    printf("%05" PRId32 " | ", diag->pos.line);
    MidPrint_line(diag->line);
    putchar('\n');

    i32 n_digits = MID_MAX(log10(diag->pos.line) + 1, 5);
    for (mid_isize i = 0; i < n_digits; ++i)
        putchar(' ');
    printf(" | ");
    MidPrint_column_arrow(diag->pos.column);
}

struct MidDiag_Diag MidDiag_expected_token_err(const char *name,
                                    const struct MidLexer_Token *tok,
                                    enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("expected %s", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

struct MidDiag_Diag MidDiag_expected_token_warn(const char *name,
                                     const struct MidLexer_Token *tok,
                                     enum MidDiag_WarnT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("expected %s", name),
        .warn = type,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

struct MidDiag_Diag MidDiag_unexpected_token_err(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("unexpected %s", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

struct MidDiag_Diag MidDiag_unexpected_token_warn(const char *name,
                                       const struct MidLexer_Token *tok,
                                       enum MidDiag_WarnT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("unexpected %s", name),
        .warn = type,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

struct MidDiag_Diag MidDiag_ident_redefined_err(const char *name,
                                     const struct MidLexer_Token *tok,
                                     enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("'%s' redefined", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

struct MidDiag_Diag MidDiag_ident_redefined_warn(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_WarnT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("'%s' redefined", name),
        .warn = type,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

struct MidDiag_Diag MidDiag_ident_undeclared_err(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("undeclared identifier '%s'", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

struct MidDiag_Diag MidDiag_ident_undeclared_warn(const char *name,
                                       const struct MidLexer_Token *tok,
                                       enum MidDiag_WarnT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("undeclared identifier '%s'", name),
        .warn = type,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

struct MidDiag_Diag MidDiag_func_undeclared_err(const char *name,
                                     const struct MidLexer_Token *tok,
                                     enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("undeclared function '%s'", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

struct MidDiag_Diag MidDiag_func_undeclared_warn(const char *name,
                                      const struct MidLexer_Token *tok,
                                      enum MidDiag_WarnT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("undeclared function '%s'", name),
        .warn = type,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

struct MidDiag_Diag MidDiag_type_id_w_name_err(const struct MidLexer_Token *tok,
                                    enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("type-id can't be named"),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

struct MidDiag_Diag MidDiag_type_id_w_name_warn(const struct MidLexer_Token *tok,
                                     enum MidDiag_WarnT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("type-id can't be named"),
        .warn = type,
        .type = MIDDIAG_TYPE_WARNING,
    };
}
