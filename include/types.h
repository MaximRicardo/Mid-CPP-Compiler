#pragma once

// info on the various primitive types in c++
// based on lp64 where:
// char is        1 byte,
// short is       2 bytes,
// int is         4 bytes,
// long is        8 bytes,
// long long is   8 bytes

// NOTE: types larger than 8 bytes are not supported

#include "ints.h"
#include "limits.h"

constexpr i32 midtype_bool_size = 1;

// DO NOT CHANGE THESE SIZES OR THE SUS IMPOSTOR AMOGUS WILL FIND YOU
constexpr i32 midtype_float_size = 4;
constexpr i32 midtype_double_size = 8;
constexpr i32 midtype_longdouble_size = 8;

typedef i8 TypesCharType;
constexpr i32 midtype_char_size = 1;
constexpr bool midtype_char_signed = true;
constexpr i64 midtype_char_smax = INT8_MAX;
constexpr i64 midtype_char_smin = INT8_MIN;
constexpr u64 midtype_char_umax = UINT8_MAX;

typedef u32 TypesWCharType;
constexpr i32 midtype_wchar_size = 4;
constexpr bool midtype_wchar_signed = false;
// limits if wchar_t were signed
constexpr i64 midtype_wchar_smax = INT32_MAX;
constexpr i64 midtype_wchar_smin = INT32_MIN;
// limits if wchar_t were unsigned
constexpr u64 midtype_wchar_umax = UINT32_MAX;
// the actual limits
constexpr i64 midtype_wchar_max =
    midtype_wchar_signed ? midtype_wchar_smax : midtype_wchar_umax;
constexpr i64 midtype_wchar_min = midtype_wchar_signed ? midtype_wchar_smin : 0;

constexpr i32 midtype_short_size = 2;
constexpr i64 midtype_short_smax = INT16_MAX;
constexpr i64 midtype_short_smin = INT16_MIN;
constexpr u64 midtype_short_umax = UINT16_MAX;

constexpr i32 midtype_int_size = 4;
constexpr i64 midtype_int_smax = INT32_MAX;
constexpr i64 midtype_int_smin = INT32_MIN;
constexpr u64 midtype_int_umax = UINT32_MAX;

constexpr i32 midtype_long_size = 8;
constexpr i64 midtype_long_smax = INT64_MAX;
constexpr i64 midtype_long_smin = INT64_MIN;
constexpr u64 midtype_long_umax = UINT64_MAX;

constexpr i32 midtype_longlong_size = 8;
constexpr i64 midtype_longlong_smax = INT64_MAX;
constexpr i64 midtype_longlong_smin = INT64_MIN;
constexpr u64 midtype_longlong_umax = UINT64_MAX;
