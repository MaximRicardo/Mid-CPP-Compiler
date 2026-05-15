#pragma once

#include "ints.h"
#include <stdio.h>

// converts a potentially multi byte utf8 encoding to a u32
u32 UTF8_read_char(const char *src, isize_t start, isize_t *out_end);

void UTF8_fprint_char(FILE *out, u32 c);
void UTF8_print_char(u32 c);
char *UTF8_char_to_str(u32 c);

void UTF8_fprint_str32(FILE *out, u32 *str);
void UTF8_fprint_str16(FILE *out, u16 *str);
void UTF8_print_str32(u32 *str);
void UTF8_print_str16(u16 *str);
char *UTF8_str32_to_str(u32 *str);
char *UTF8_str16_to_str(u16 *str);
