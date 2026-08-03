#pragma once

#include "ints.h"

struct mid_Position {
    const char *file;
    i32 line, column;
};

bool mid_position_equal(const struct mid_Position *a,
                        const struct mid_Position *b);
