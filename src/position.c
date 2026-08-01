#include "position.h"
#include <string.h>

bool Mid_position_equal(const struct Mid_Position *a,
                        const struct Mid_Position *b)
{
    return a->line == b->line && a->column == b->column &&
           !strcmp(a->file, b->file);
}
