#pragma once

#define SWAP(x, y)                                                             \
    do {                                                                       \
        typeof(x) tmp_super_specific_name______ = x;                           \
        x = y;                                                                 \
        y = tmp_super_specific_name______;                                     \
    } while (0)
