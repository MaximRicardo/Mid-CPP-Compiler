#include "allocator.h"
#include "generics/bumpalloc.h"
#include "scope.h"

void midllvm_Allocators_deinit(struct midllvm_Allocators *allocs)
{
    midgen_bumpdeinit(&allocs->scope, midllvm_Scope_deinit);
}
