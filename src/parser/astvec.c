#include "astvec.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/ast.h"

struct Parser_ASTNodePVec Parser_copy_nodepvec(
    const struct Parser_ASTNodePVec *src, struct Parser_ASTNode *dest_parent,
    struct Sema_Scope *dest_scope, struct Parser_Allocators *allocs)
{
    struct Parser_ASTNodePVec ret = {};
    gen_dynreserve(&ret, src->len);

    for (isize_t i = 0; i < src->len; ++i) {
        struct Parser_ASTNode *cpy;
        gen_bumpmalloc(&allocs->ast, &cpy);

        Parser_copy_node(cpy, src->arr[i], dest_parent, dest_scope, allocs);

        gen_dynpush(&ret, cpy);
    }

    return ret;
}
