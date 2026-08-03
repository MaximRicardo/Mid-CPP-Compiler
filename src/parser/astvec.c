#include "parser/astvec.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/ast.h"

struct midpar_ASTNodePVec midpar_copy_nodepvec(
    const struct midpar_ASTNodePVec *src, struct midpar_ASTNode *dest_parent,
    struct midsema_Scope *dest_scope, struct midpar_Allocators *allocs)
{
    struct midpar_ASTNodePVec ret = {};
    midgen_dynreserve(&ret, src->len);

    for (mid_isize i = 0; i < src->len; ++i) {
        struct midpar_ASTNode *cpy;
        midgen_bumpmalloc(&allocs->ast, &cpy);

        midpar_copy_node(cpy, src->arr[i], dest_parent, dest_scope, allocs);

        midgen_dynpush(&ret, cpy);
    }

    return ret;
}
