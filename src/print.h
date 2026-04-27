#pragma once

#include "attribute.h"

// prints until a '\n' character
void Print_line(const char *line);
char *Print_fmt_to_str(const char *fmt, ...) ATTRIBUTE((format(printf, 1, 2)));
