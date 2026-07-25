#pragma once

#include "parser/ast.h"

struct Parser_Type
Sema_instantiate_class_tmplt(struct Parser_ASTNode *tmplt,
                             const struct Parser_TmpltArgVec *args,
                             struct Parser_Allocators *allocs);
