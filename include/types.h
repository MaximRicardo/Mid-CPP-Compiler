#pragma once

// info on the various primitive types in c++
// based on lp64 where:
// char is        1 byte,
// short is       2 bytes,
// int is         4 bytes,
// long is        8 bytes,
// long long is   8 bytes

// NOTE: types larger than 8 bytes are not supported

#include "apfloat.h"
#include "apint.h"
#include "limits.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// inits all the APInts
void midtype_init_module();

constexpr int32_t midtype_bool_size = 1;

constexpr int32_t midtype_float_size = 4;
constexpr int32_t midtype_double_size = 8;
constexpr int32_t midtype_longdouble_size = 8;

constexpr enum midflt_Kind midtype_float_kind = MIDFLT_KIND_IEEE_SINGLE;
constexpr enum midflt_Kind midtype_double_kind = MIDFLT_KIND_IEEE_DOUBLE;
constexpr enum midflt_Kind midtype_longdouble_kind = MIDFLT_KIND_IEEE_DOUBLE;

typedef int8_t TypesCharType;
constexpr int32_t midtype_char_size = 1;
constexpr bool midtype_char_signed = true;
const struct mid_APInt *midtype_char_smax(), *midtype_char_smin(),
    *midtype_char_umax();

typedef uint32_t TypesWCharType;
constexpr int32_t midtype_wchar_size = 4;
constexpr bool midtype_wchar_signed = false;
const struct mid_APInt *midtype_wchar_smax(), *midtype_wchar_smin(),
    *midtype_wchar_umax();

constexpr int32_t midtype_short_size = 2;
const struct mid_APInt *midtype_short_smax(), *midtype_short_smin(),
    *midtype_short_umax();

constexpr int32_t midtype_int_size = 4;
const struct mid_APInt *midtype_int_smax(), *midtype_int_smin(),
    *midtype_int_umax();

constexpr int32_t midtype_long_size = 8;
const struct mid_APInt *midtype_long_smax(), *midtype_long_smin(),
    *midtype_long_umax();

constexpr int32_t midtype_longlong_size = 8;
const struct mid_APInt *midtype_longlong_smax(), *midtype_longlong_smin(),
    *midtype_longlong_umax();

constexpr int32_t midtype_ptr_size = 8;
const struct mid_APInt *midtype_ptr_smax(), *midtype_ptr_smin(),
    *midtype_ptr_umax();

#ifdef __cplusplus
}
#endif
