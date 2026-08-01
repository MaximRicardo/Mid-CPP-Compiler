#include "enum.h"
#include "ast.h"
#include "generics/dynarray.h"
#include "parser/astvec.h"

void MidParser_Enum_deinit(struct MidParser_Enum *self)
{
    MidGen_dyndeinit(&self->nodes);
}

void MidParser_copy_enum(struct MidParser_Enum *dest,
                         const struct MidParser_Enum *src,
                         struct MidSema_Scope *dest_scope,
                         struct MidParser_Allocators *allocs)
{
    *dest = *src;
    dest->nodes = MidParser_copy_nodepvec(&src->nodes, MIDPARSER_GET_NODE(dest),
                                          dest_scope, allocs);
}
