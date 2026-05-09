#pragma once

#include "attribute.h"
#include "ints.h"

// prints until a '\n' character
void Print_line(const char *line);
char *Print_fmt_to_str(const char *fmt, ...) ATTRIBUTE((format(printf, 1, 2)));
// prints a line with an arrow at the specified column
// NOTE: doesn't account for unicode stuff
void Print_column_arrow(i32 column);
