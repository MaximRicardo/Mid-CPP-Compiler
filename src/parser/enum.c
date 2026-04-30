#include "enum.h"
#include "generics/dynarray.h"
#include "parser/ast.h"

void Parser_Enum_deinit(struct Parser_Enum *self)
{
    gen_dyndeinit(&self->nodes, Parser_ASTNodeP_deinit);
}
