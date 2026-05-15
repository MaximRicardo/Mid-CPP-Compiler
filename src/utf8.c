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

static struct UnicodeChar read_char_2_byte(const char *src, isize_t start)
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

static struct UnicodeChar read_char_3_byte(const char *src, isize_t start)
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

static struct UnicodeChar read_char_4_byte(const char *src, isize_t start)
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

static u32 uni_to_c32(struct UnicodeChar uni)
{
    u32 ret = 0;
    ret |= (u32)uni.z;
    ret |= (u32)uni.y << 4;
    ret |= (u32)uni.x << 8;
    ret |= (u32)uni.w << 12;
    ret |= (u32)uni.v << 16;
    ret |= (u32)uni.u << 20;
    return ret;
}

u32 UTF8_read_char(const char *src, isize_t start, isize_t *out_end)
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
        CRASH("bad UTF-8 character encoding");
    }

    return uni_to_c32(ret);
}

void UTF8_fprint_char(FILE *out, u32 c)
{
    char buf[MB_LEN_MAX + 1] = {0};
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));

    c32rtomb(buf, c, &ps);
    fprintf(out, "%s", buf);
}

void UTF8_print_char(u32 c)
{
    UTF8_fprint_char(stdout, c);
}

char *UTF8_char_to_str(u32 c)
{
    char *ret = calloc(MB_LEN_MAX + 1, 1);
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));

    c32rtomb(ret, c, &ps);

    return ret;
}

void UTF8_fprint_str32(FILE *out, u32 *str)
{
    for (isize_t i = 0; str[i] != '\0'; ++i)
        UTF8_fprint_char(out, str[i]);
}

void UTF8_print_str32(u32 *str)
{
    UTF8_fprint_str32(stdout, str);
}

void UTF8_fprint_str16(FILE *out, u16 *str)
{
    for (isize_t i = 0; str[i] != '\0'; ++i)
        UTF8_fprint_char(out, str[i]);
}

void UTF8_print_str16(u16 *str)
{
    UTF8_fprint_str16(stdout, str);
}

char *UTF8_str32_to_str(u32 *str)
{
    struct Dynstr ret = {};

    for (isize_t i = 0; str[i] != '\0'; ++i) {
        char *c = UTF8_char_to_str(str[i]);
        Dynstr_append(&ret, c);
        free(c);
    }

    return ret.str;
}

char *UTF8_str16_to_str(u16 *str)
{
    struct Dynstr ret = {};

    for (isize_t i = 0; str[i] != '\0'; ++i) {
        char *c = UTF8_char_to_str(str[i]);
        Dynstr_append(&ret, c);
        free(c);
    }

    return ret.str;
}
