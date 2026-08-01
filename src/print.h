#pragma once

#include "attribute.h"
#include "ints.h"

constexpr char midprt_ansi_reset[] = "\x1b[0m";
constexpr char midprt_ansi_red[] = "\x1b[31m";
constexpr char midprt_ansi_green[] = "\x1b[32m";
constexpr char midprt_ansi_yellow[] = "\x1b[33m";
constexpr char midprt_ansi_blue[] = "\x1b[34m";
constexpr char midprt_ansi_magenta[] = "\x1b[35m";
constexpr char midprt_ansi_cyan[] = "\x1b[36m";

// prints until a '\n' character
void midprt_line(const char *line);
char *midprt_fmt_to_str(const char *fmt, ...)
    MID_ATTRIBUTE((format(printf, 1, 2)));
// prints a line with an arrow at the specified column
// NOTE: doesn't account for unicode stuff
void midprt_column_arrow(i32 column);
