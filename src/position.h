#pragma once

#include "ints.h"

struct Position {
    const char *file;
    i32 line, column;
};

bool Position_equal(const struct Position *a, const struct Position *b);
