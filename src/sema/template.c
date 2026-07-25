#include "template.h"
#include "generics/bumpalloc.h"
#include "macros.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/template.h"

struct Parser_Type
Sema_instantiate_class_tmplt(struct Parser_ASTNode *tmplt_node,
                             struct Parser_TmpltArgVec *args,
                             struct Parser_Allocators *allocs)
{
    (void)args;
    auto tmplt = &tmplt_node->tmplt;
    assert(tmplt->child->type == PARSER_ASTNODETYPE_CLASS);

    struct Parser_TmpltInst inst = {};
    gen_bumpmalloc(&allocs->scope, &inst.scope);
    gen_bumpmalloc(&allocs->ast, &inst.inst);

    *inst.scope = (struct Sema_Scope){.parent = tmplt->scope,
                                      .node = inst.inst,
                                      .type = SEMA_SCOPETYPE_TEMPLATE_INST};

    printf("copying class node\n");
    Parser_copy_node(inst.inst, tmplt->child, tmplt_node, inst.scope, allocs);
    printf("copy done\n");

    CRASH("class copy done");
}
