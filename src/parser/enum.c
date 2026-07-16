#include "enum.h"
#include "ast.h"
#include "generics/dynarray.h"
#include "parser/astvec.h"

void Parser_Enum_deinit(struct Parser_Enum *self)
{
    gen_dyndeinit(&self->nodes);
}

void Parser_copy_enum(struct Parser_ASTNode *dest_node,
                      const struct Parser_ASTNode *src_node,
                      struct Sema_Scope *dest_scope,
                      struct Parser_Allocators *allocs)
{
    auto dest = &dest_node->enum_;
    auto src = &src_node->enum_;

    *dest = *src;
    dest->nodes =
        Parser_copy_nodepvec(&src->nodes, dest_node, dest_scope, allocs);
}
