#include "allocator.h"
#include "generics/bumpalloc.h"
#include "scope.h"

void MidLLVM_Allocators_deinit(struct MidLLVM_Allocators *allocs)
{
    MidGen_bumpdeinit(&allocs->scope, MidLLVM_Scope_deinit);
}
