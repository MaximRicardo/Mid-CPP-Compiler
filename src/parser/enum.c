#include "parser/enum.h"
#include "generics/dynarray.h"
#include "parser/ast.h"
#include "parser/astvec.h"

void midpar_Enum_deinit(struct midpar_Enum *self)
{
    midgen_dyndeinit(&self->nodes);
}

void midpar_copy_enum(struct midpar_Enum *dest, const struct midpar_Enum *src,
                      struct midsema_Scope *dest_scope,
                      struct midpar_Allocators *allocs)
{
    *dest = *src;
    dest->nodes = midpar_copy_nodepvec(&src->nodes, MIDPAR_GET_NODE(dest),
                                       dest_scope, allocs);
}
