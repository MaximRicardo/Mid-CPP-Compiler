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

// measured in bytes
constexpr i32 Types_char_size = 1;
constexpr i32 Types_short_size = 2;
constexpr i32 Types_int_size = 4;
constexpr i32 Types_long_size = 8;
constexpr i32 Types_longlong_size = 8;
constexpr i32 Types_bool_size = 4;
constexpr i32 Types_wchar_size = 4;

// DO NOT CHANGE
constexpr i32 Types_float_size = 4;
constexpr i32 Types_double_size = 8;
constexpr i32 Types_longdouble_size = 8;

constexpr bool Types_char_signed = true;
constexpr bool Types_wchar_signed = false;

constexpr i64 Types_char_smax = INT8_MAX;
constexpr i64 Types_char_smin = INT8_MIN;
constexpr u64 Types_char_umax = UINT8_MAX;

constexpr i64 Types_short_smax = INT16_MAX;
constexpr i64 Types_short_smin = INT16_MIN;
constexpr u64 Types_short_umax = UINT16_MAX;

constexpr i64 Types_int_smax = INT32_MAX;
constexpr i64 Types_int_smin = INT32_MIN;
constexpr u64 Types_int_umax = UINT32_MAX;

constexpr i64 Types_long_smax = INT64_MAX;
constexpr i64 Types_long_smin = INT64_MIN;
constexpr u64 Types_long_umax = UINT64_MAX;

constexpr i64 Types_longlong_smax = INT64_MAX;
constexpr i64 Types_longlong_smin = INT64_MIN;
constexpr u64 Types_longlong_umax = UINT64_MAX;
