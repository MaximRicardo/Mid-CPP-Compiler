#include "allocator.h"
#include "generics/bumpalloc.h"
#include "scope.h"

void CGLLVM_Allocators_deinit(struct CGLLVM_Allocators *allocs)
{
    gen_bumpdeinit(&allocs->scope, CGLLVM_Scope_deinit);
}
