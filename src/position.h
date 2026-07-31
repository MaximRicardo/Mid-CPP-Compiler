#pragma once

#include "ints.h"

struct Mid_Position {
    const char *file;
    i32 line, column;
};

bool Mid_position_equal(const struct Mid_Position *a, const struct Mid_Position *b);
