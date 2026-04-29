#pragma once

#include "parser/astvec.h"

struct Parser_Namespace {
    struct Parser_ASTNodeVec nodes;
    const char *name; // NULL for anonymous namespaces
};

void Parser_Namespace_deinit(struct Parser_Namespace *self);
