#include "enum.h"
#include "ast.h"
#include "generics/dynarray.h"
#include "parser/astvec.h"

void Parser_Enum_deinit(struct Parser_Enum *self)
{
    gen_dyndeinit(&self->nodes);
}

void Parser_copy_enum(struct Parser_Enum *dest, const struct Parser_Enum *src,
                      struct Sema_Scope *dest_scope,
                      struct Parser_Allocators *allocs)
{
    *dest = *src;
    dest->nodes = Parser_copy_nodepvec(&src->nodes, PARSER_GET_NODE(dest),
                                       dest_scope, allocs);
}
