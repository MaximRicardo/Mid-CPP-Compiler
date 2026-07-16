#pragma once

#include "parser/astvec.h"

struct Parser_Enum {
    struct Parser_ASTNodePVec nodes;
    const char *name;
    bool is_enumclass;
};

void Parser_Enum_deinit(struct Parser_Enum *self);
void Parser_copy_enum(struct Parser_ASTNode *dest,
                      const struct Parser_ASTNode *src,
                      struct Sema_Scope *dest_scope,
                      struct Parser_Allocators *allocs);
