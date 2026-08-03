#pragma once

#include "attribute.h"
#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

constexpr char midcmd_ansi_reset[] = "\x1b[0m";
constexpr char midcmd_ansi_red[] = "\x1b[31m";
constexpr char midcmd_ansi_green[] = "\x1b[32m";
constexpr char midcmd_ansi_yellow[] = "\x1b[33m";
constexpr char midcmd_ansi_blue[] = "\x1b[34m";
constexpr char midcmd_ansi_magenta[] = "\x1b[35m";
constexpr char midcmd_ansi_cyan[] = "\x1b[36m";

struct midcmd_Args {
    const char *src;
    const char *ast_out;
    const char *asm_out;
    bool log_tokens;
    bool log_symbols;
};

void midcmd_init_args(int argc, char **argv);
const struct midcmd_Args *midcmd_get_args(void);

// prints until a '\n' character
void midcmd_prt_line(const char *line);
// like printf but returns the string that would have been printed to the
// console.
// NOTE: returned string must be free'd
char *midcmd_fmt_to_str(const char *fmt, ...)
    MID_ATTRIBUTE((format(printf, 1, 2)));
// prints a line with an arrow at the specified column
// NOTE: doesn't account for unicode stuff
void midcmd_prt_column_arrow(i32 column);

#ifdef __cplusplus
}
#endif
