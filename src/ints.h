#pragma once

// stdint type names are too long and annoying and im lazy

#include <inttypes.h>

typedef int8_t i8;
typedef uint8_t u8;

typedef int16_t i16;
typedef uint16_t u16;

typedef int32_t i32;
typedef uint32_t u32;

typedef int64_t i64;
typedef uint64_t u64;

// good nuff
typedef i64 isize_t;
#define PRIisz PRId64
#define ISIZE_MAX INT64_MAX
#define ISIZE_MIN INT64_MIN
