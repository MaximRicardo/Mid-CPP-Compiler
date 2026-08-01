#pragma once

#include "parser/ast.h"

struct MidParser_Type
MidSema_instantiate_class_tmplt(struct MidParser_ASTNode *tmplt,
                                const struct MidParser_TmpltArgVec *args,
                                struct MidParser_Allocators *allocs);
