#pragma once

#include "parser/astvec.h"

struct Parser_Namespace {
    struct Parser_ASTNodePVec nodes;
    const char *name; // NULL for anonymous namespaces
};

void Parser_Namespace_deinit(struct Parser_Namespace *self);
