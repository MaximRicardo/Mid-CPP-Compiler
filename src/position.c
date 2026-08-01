#include "position.h"
#include <string.h>

bool mid_position_equal(const struct mid_Position *a,
                        const struct mid_Position *b)
{
    return a->line == b->line && a->column == b->column &&
           !strcmp(a->file, b->file);
}
