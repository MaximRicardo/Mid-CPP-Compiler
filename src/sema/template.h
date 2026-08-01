#pragma once

#include "parser/ast.h"

struct midpar_Type
midsema_instantiate_class_tmplt(struct midpar_ASTNode *tmplt,
                                const struct midpar_TmpltArgVec *args,
                                struct midpar_Allocators *allocs);
