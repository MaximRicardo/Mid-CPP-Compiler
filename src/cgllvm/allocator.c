#include "cgllvm/allocator.h"
#include "cgllvm/scope.h"
#include "generics/bumpalloc.h"

void midllvm_Allocators_deinit(struct midllvm_Allocators *allocs)
{
    midgen_bumpdeinit(&allocs->scope, midllvm_Scope_deinit);
}
