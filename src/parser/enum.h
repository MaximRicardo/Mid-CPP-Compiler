#pragma once

#include "parser/astvec.h"

struct MidParser_Enum {
    struct MidParser_ASTNodePVec nodes;
    const char *name;
    bool is_enumclass;
};

void MidParser_Enum_deinit(struct MidParser_Enum *self);
void MidParser_copy_enum(struct MidParser_Enum *dest, const struct MidParser_Enum *src,
                      struct MidSema_Scope *dest_scope,
                      struct MidParser_Allocators *allocs);
