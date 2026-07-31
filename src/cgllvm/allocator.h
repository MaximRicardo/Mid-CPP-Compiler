#pragma once

#include "generics/bumpalloc.h"

MidGen_bumpalloc_struct_named(MidLLVM_ScopeBump, struct MidLLVM_Scope);

struct MidLLVM_Allocators {
    struct MidLLVM_ScopeBump scope;
};

void MidLLVM_Allocators_deinit(struct MidLLVM_Allocators *allocs);
