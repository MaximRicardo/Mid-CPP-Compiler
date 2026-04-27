#pragma once

#include "ints.h"

struct Position {
    const char *file;
    i32 line, column;
};
