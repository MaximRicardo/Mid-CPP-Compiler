#pragma once

#include "ints.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// converts a potentially multi byte utf8 encoding to a uint32_t
uint32_t midutf8_read_char(const char *src, mid_isize start,
                           mid_isize *out_end);

void midutf8_fprint_char(FILE *out, uint32_t c);
void midutf8_print_char(uint32_t c);
char *midutf8_char_to_str(uint32_t c);

void midutf8_fprint_str32(FILE *out, uint32_t *str);
void midutf8_fprint_str16(FILE *out, uint16_t *str);
void midutf8_print_str32(uint32_t *str);
void midutf8_print_str16(uint16_t *str);
char *midutf8_str32_to_str(uint32_t *str);
char *midutf8_str16_to_str(uint16_t *str);

#ifdef __cplusplus
}
#endif
