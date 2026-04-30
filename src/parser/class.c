#include "class.h"
#include "generics/dynarray.h"
#include "parser/ast.h"

void Parser_Class_deinit(struct Parser_Class *self)
{
    gen_dyndeinit(&self->nodes, Parser_ASTNodeP_deinit);
}
