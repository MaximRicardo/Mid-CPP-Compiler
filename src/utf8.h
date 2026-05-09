#pragma once

#include "ints.h"

// converts a potentially multi byte utf8 encoding to a u32
u32 UTF8_read_char(const char *src, isize_t start, isize_t *out_end);

void UTF8_print_char(u32 c);
char *UTF8_to_str(u32 c);
