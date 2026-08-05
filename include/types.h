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
#include "ints.h"
#include "limits.h"

#ifdef __cplusplus
extern "C" {
#endif

constexpr int32_t midtype_bool_size = 1;

constexpr int32_t midtype_float_size = 4;
constexpr int32_t midtype_double_size = 8;
constexpr int32_t midtype_longdouble_size = 8;

constexpr enum midflt_Rounding midtype_default_rmode =
    MIDFLT_ROUND_NEAREST_TIES_EVEN;
constexpr enum midflt_Kind midtype_float_kind = MIDFLT_KIND_IEEE_SINGLE;
constexpr enum midflt_Kind midtype_double_kind = MIDFLT_KIND_IEEE_DOUBLE;
constexpr enum midflt_Kind midtype_longdouble_kind = MIDFLT_KIND_IEEE_DOUBLE;

typedef int8_t TypesCharType;
constexpr int32_t midtype_char_size = 1;
constexpr bool midtype_char_signed = true;
constexpr int64_t midtype_char_smax = INT8_MAX;
constexpr int64_t midtype_char_smin = INT8_MIN;
constexpr uint64_t midtype_char_umax = UINT8_MAX;

typedef uint32_t TypesWCharType;
constexpr int32_t midtype_wchar_size = 4;
constexpr bool midtype_wchar_signed = false;
// limits if wchar_t were signed
constexpr int64_t midtype_wchar_smax = INT32_MAX;
constexpr int64_t midtype_wchar_smin = INT32_MIN;
// limits if wchar_t were unsigned
constexpr uint64_t midtype_wchar_umax = UINT32_MAX;
// the actual limits
constexpr int64_t midtype_wchar_max =
    midtype_wchar_signed ? midtype_wchar_smax : midtype_wchar_umax;
constexpr int64_t midtype_wchar_min =
    midtype_wchar_signed ? midtype_wchar_smin : 0;

constexpr int32_t midtype_short_size = 2;
constexpr int64_t midtype_short_smax = INT16_MAX;
constexpr int64_t midtype_short_smin = INT16_MIN;
constexpr uint64_t midtype_short_umax = UINT16_MAX;

constexpr int32_t midtype_int_size = 4;
constexpr int64_t midtype_int_smax = INT32_MAX;
constexpr int64_t midtype_int_smin = INT32_MIN;
constexpr uint64_t midtype_int_umax = UINT32_MAX;

constexpr int32_t midtype_long_size = 8;
constexpr int64_t midtype_long_smax = INT64_MAX;
constexpr int64_t midtype_long_smin = INT64_MIN;
constexpr uint64_t midtype_long_umax = UINT64_MAX;

constexpr int32_t midtype_longlong_size = 8;
constexpr int64_t midtype_longlong_smax = INT64_MAX;
constexpr int64_t midtype_longlong_smin = INT64_MIN;
constexpr uint64_t midtype_longlong_umax = UINT64_MAX;

constexpr int32_t midtype_ptr_size = 8;
constexpr uint64_t midtype_ptr_umax = UINT64_MAX;

#ifdef __cplusplus
}
#endif
