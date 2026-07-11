#include "position.h"
#include <string.h>

bool Position_equal(const struct Position *a, const struct Position *b)
{
    return a->line == b->line && a->column == b->column &&
           !strcmp(a->file, b->file);
}
