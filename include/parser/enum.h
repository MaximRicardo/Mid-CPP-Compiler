#pragma once

#include "parser/astvec.h"

struct midpar_Enum {
    struct midpar_ASTNodePVec nodes;
    const char *name;
    bool is_enumclass;
};

void midpar_Enum_deinit(struct midpar_Enum *self);
void midpar_copy_enum(struct midpar_Enum *dest, const struct midpar_Enum *src,
                      struct midsema_Scope *dest_scope,
                      struct midpar_Allocators *allocs);
