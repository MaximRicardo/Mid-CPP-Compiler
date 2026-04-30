#pragma once

#include "parser/astvec.h"

// classes, structs and unions
struct Parser_Class {
    struct Parser_ASTNodePVec nodes;
    const char *name;
    bool is_union;
};

void Parser_Class_deinit(struct Parser_Class *self);
