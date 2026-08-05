#include "utf8.h"
#include "dynstr.h"
#include "ints.h"
#include "macros.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>

struct UnicodeChar {
    unsigned z : 4;
    unsigned y : 4;
    unsigned x : 4;
    unsigned w : 4;
    unsigned v : 4;
    unsigned u : 4;
};

static struct UnicodeChar read_char_2_byte(const char *src, mid_isize start)
{
    // src[start + 0] = 0b110xxxyy
    // src[start + 1] = 0b10yyzzzz

    struct UnicodeChar ret = {};

    ret.z = src[start + 1] & 0b00001111;

    ret.y = (src[start + 1] & 0b00110000) >> 4;
    ret.y |= (src[start] & 0b00000011) << 2;

    ret.x = (src[start] & 0b00011100) >> 2;

    return ret;
}

static struct UnicodeChar read_char_3_byte(const char *src, mid_isize start)
{
    // src[start + 0] = 0b1110wwww
    // src[start + 1] = 0b10xxxxyy
    // src[start + 2] = 0b10yyzzzz

    struct UnicodeChar ret = {};

    ret.z = src[start + 2] & 0b00001111;

    ret.y = (src[start + 2] & 0b00110000) >> 4;
    ret.y |= (src[start + 1] & 0b00000011) << 2;

    ret.x = (src[start + 1] & 0b00111100) >> 2;

    ret.w = src[start] & 0b00001111;

    return ret;
}

static struct UnicodeChar read_char_4_byte(const char *src, mid_isize start)
{
    // src[start + 0] = 0b11110uvv
    // src[start + 1] = 0b10vvwwww
    // src[start + 2] = 0b10xxxxyy
    // src[start + 3] = 0b10yyzzzz

    struct UnicodeChar ret = {};

    ret.z = src[start + 3] & 0b00001111;

    ret.y = (src[start + 3] & 0b00110000) >> 4;
    ret.y |= (src[start + 2] & 0b00000011) << 2;

    ret.x = (src[start + 2] & 0b00111100) >> 2;

    ret.w = src[start + 1] & 0b00001111;

    ret.v = (src[start + 1] & 0b00110000) >> 4;
    ret.v |= (src[start] & 0b00000011) << 2;

    ret.u = (src[start] & 0b00000100) >> 2;

    return ret;
}

static uint32_t uni_to_c32(struct UnicodeChar uni)
{
    uint32_t ret = 0;
    ret |= (uint32_t)uni.z;
    ret |= (uint32_t)uni.y << 4;
    ret |= (uint32_t)uni.x << 8;
    ret |= (uint32_t)uni.w << 12;
    ret |= (uint32_t)uni.v << 16;
    ret |= (uint32_t)uni.u << 20;
    return ret;
}

uint32_t midutf8_read_char(const char *src, mid_isize start, mid_isize *out_end)
{
    unsigned char b0 = src[start];

    struct UnicodeChar ret;

    if (b0 < 128) {
        if (out_end)
            *out_end = start + 1;
        return src[start];
    } else if ((b0 & 0b11100000) == 0b11000000) { // b0 == 0b110xxxxx
        if (out_end)
            *out_end = start + 2;
        ret = read_char_2_byte(src, start);
    } else if ((b0 & 0b11110000) == 0b11100000) { // b0 == 0b1110xxxx
        if (out_end)
            *out_end = start + 3;
        ret = read_char_3_byte(src, start);
    } else if ((b0 & 0b11111000) == 0b11110000) { // b0 == 0b11110xxx
        if (out_end)
            *out_end = start + 4;
        ret = read_char_4_byte(src, start);
    } else {
        MID_CRASH("bad UTF-8 character encoding");
    }

    return uni_to_c32(ret);
}

void midutf8_fprint_char(FILE *out, uint32_t c)
{
    char buf[MB_LEN_MAX + 1] = {0};
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));

    c32rtomb(buf, c, &ps);
    fprintf(out, "%s", buf);
}

void midutf8_print_char(uint32_t c)
{
    midutf8_fprint_char(stdout, c);
}

char *midutf8_char_to_str(uint32_t c)
{
    char *ret = calloc(MB_LEN_MAX + 1, 1);
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));

    c32rtomb(ret, c, &ps);

    return ret;
}

void midutf8_fprint_str32(FILE *out, uint32_t *str)
{
    for (mid_isize i = 0; str[i] != '\0'; ++i)
        midutf8_fprint_char(out, str[i]);
}

void midutf8_print_str32(uint32_t *str)
{
    midutf8_fprint_str32(stdout, str);
}

void midutf8_fprint_str16(FILE *out, uint16_t *str)
{
    for (mid_isize i = 0; str[i] != '\0'; ++i)
        midutf8_fprint_char(out, str[i]);
}

void midutf8_print_str16(uint16_t *str)
{
    midutf8_fprint_str16(stdout, str);
}

char *midutf8_str32_to_str(uint32_t *str)
{
    struct mid_Dynstr ret = {};

    for (mid_isize i = 0; str[i] != '\0'; ++i) {
        char *c = midutf8_char_to_str(str[i]);
        midstr_append(&ret, c);
        free(c);
    }

    return ret.str;
}

char *midutf8_str16_to_str(uint16_t *str)
{
    struct mid_Dynstr ret = {};

    for (mid_isize i = 0; str[i] != '\0'; ++i) {
        char *c = midutf8_char_to_str(str[i]);
        midstr_append(&ret, c);
        free(c);
    }

    return ret.str;
}
