#pragma once

#include "ints.h"
#include <stdio.h>

// converts a potentially multi byte utf8 encoding to a u32
u32 midutf8_read_char(const char *src, mid_isize start, mid_isize *out_end);

void midutf8_fprint_char(FILE *out, u32 c);
void midutf8_print_char(u32 c);
char *midutf8_char_to_str(u32 c);

void midutf8_fprint_str32(FILE *out, u32 *str);
void midutf8_fprint_str16(FILE *out, u16 *str);
void midutf8_print_str32(u32 *str);
void midutf8_print_str16(u16 *str);
char *midutf8_str32_to_str(u32 *str);
char *midutf8_str16_to_str(u16 *str);
