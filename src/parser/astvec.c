#include "astvec.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/ast.h"

struct MidParser_ASTNodePVec
MidParser_copy_nodepvec(const struct MidParser_ASTNodePVec *src,
                        struct MidParser_ASTNode *dest_parent,
                        struct MidSema_Scope *dest_scope,
                        struct MidParser_Allocators *allocs)
{
    struct MidParser_ASTNodePVec ret = {};
    MidGen_dynreserve(&ret, src->len);

    for (mid_isize i = 0; i < src->len; ++i) {
        struct MidParser_ASTNode *cpy;
        MidGen_bumpmalloc(&allocs->ast, &cpy);

        MidParser_copy_node(cpy, src->arr[i], dest_parent, dest_scope, allocs);

        MidGen_dynpush(&ret, cpy);
    }

    return ret;
}
