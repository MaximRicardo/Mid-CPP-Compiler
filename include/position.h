#pragma once

#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mid_Position {
    const char *file;
    i32 line, column;
};

bool mid_position_equal(const struct mid_Position *a,
                        const struct mid_Position *b);

#ifdef __cplusplus
}
#endif
