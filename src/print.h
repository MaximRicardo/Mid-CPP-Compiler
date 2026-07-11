#pragma once

#include "attribute.h"
#include "ints.h"

constexpr char Print_ansi_reset[] = "\x1b[0m";
constexpr char Print_ansi_red[] = "\x1b[31m";
constexpr char Print_ansi_green[] = "\x1b[32m";
constexpr char Print_ansi_yellow[] = "\x1b[33m";
constexpr char Print_ansi_blue[] = "\x1b[34m";
constexpr char Print_ansi_magenta[] = "\x1b[35m";
constexpr char Print_ansi_cyan[] = "\x1b[36m";

// prints until a '\n' character
void Print_line(const char *line);
char *Print_fmt_to_str(const char *fmt, ...) ATTRIBUTE((format(printf, 1, 2)));
// prints a line with an arrow at the specified column
// NOTE: doesn't account for unicode stuff
void Print_column_arrow(i32 column);
