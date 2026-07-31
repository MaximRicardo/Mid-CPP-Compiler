#pragma once

#include "ints.h"
#include <stdio.h>

// converts a potentially multi byte utf8 encoding to a u32
u32 MidUTF8_read_char(const char *src, mid_isize start, mid_isize *out_end);

void MidUTF8_fprint_char(FILE *out, u32 c);
void MidUTF8_print_char(u32 c);
char *MidUTF8_char_to_str(u32 c);

void MidUTF8_fprint_str32(FILE *out, u32 *str);
void MidUTF8_fprint_str16(FILE *out, u16 *str);
void MidUTF8_print_str32(u32 *str);
void MidUTF8_print_str16(u16 *str);
char *MidUTF8_str32_to_str(u32 *str);
char *MidUTF8_str16_to_str(u16 *str);
