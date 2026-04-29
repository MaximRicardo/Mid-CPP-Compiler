#pragma once

#include "parser/astvec.h"

struct Parser_Enum {
    struct Parser_ASTNodeVec nodes;
    const char *name;
    bool is_enumclass;
};

void Parser_Enum_deinit(struct Parser_Enum *self);
