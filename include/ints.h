#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// good nuff
typedef int64_t mid_isize;
#define MID_PRIdsz PRId64
#define MID_PRIisz PRIi64
#define MID_PRIxsz PRIx64
#define MID_ISIZE_MAX INT64_MAX
#define MID_ISIZE_MIN INT64_MIN

#ifdef __cplusplus
}
#endif
