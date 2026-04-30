#include "namespace.h"
#include "generics/dynarray.h"
#include "parser/ast.h"

void Parser_Namespace_deinit(struct Parser_Namespace *self)
{
    gen_dyndeinit(&self->nodes, Parser_ASTNodeP_deinit);
}
