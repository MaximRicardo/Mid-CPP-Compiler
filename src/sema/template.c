#include "template.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/ast_log.h"
#include "parser/class.h"
#include "parser/template.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <stdio.h>

static void transf_type(struct Parser_Type *type,
                        const struct Parser_ASTNode *tmplt_node,
                        const struct Parser_TmpltArgVec *args)
{
    if (type->spec != PARSER_TYPESPEC_TEMPLATED)
        return;

    auto tmplt = &tmplt_node->tmplt;

    const char *name = Sema_deref_identptr(&type->named)->name;
    isize_t idx = Parser_tmplt_param_idx(tmplt, name);

    auto param = tmplt->params.arr[idx];
    assert(param->kind == PARSER_TMPLTPARAM_TYPE);

    auto arg = &args->arr[idx];

    // the top most CV qualifier of the templated type is discarded
    for (isize_t i = 1; i < arg->type.dquals.len; ++i) {
        gen_dynpush(&type->dquals, arg->type.dquals.arr[i]);
    }

    type->spec = arg->type.spec;

    if (arg->type.spec == PARSER_TYPESPEC_FPTR) {
        type->fptr = mid_malloc(sizeof(*type->fptr));
        *type->fptr = Parser_copy_fptr_type(arg->type.fptr);
    } else if (arg->type.spec == PARSER_TYPESPEC_ARRAY) {
        type->array = mid_malloc(sizeof(*type->array));
        *type->array = Parser_copy_array_type(arg->type.array);
    } else if (Parser_is_typespec_named(arg->type.spec)) {
        type->named = arg->type.named;
    }
}

static void transf_node(struct Parser_ASTNode *node,
                        const struct Parser_ASTNode *tmplt_node,
                        const struct Parser_TmpltArgVec *args);

static void transf_var_inst(struct Parser_VarDeclInst *inst,
                            const struct Parser_ASTNode *tmplt_node,
                            const struct Parser_TmpltArgVec *args)
{
    transf_type(&inst->type, tmplt_node, args);
}

static void transf_var(struct Parser_ASTNode *var_node,
                       const struct Parser_ASTNode *tmplt_node,
                       const struct Parser_TmpltArgVec *args)
{
    auto var = &var_node->var_decl;

    for (isize_t i = 0; i < var->insts.len; ++i) {
        auto inst = var->insts.arr[i];
        transf_var_inst(inst, tmplt_node, args);
    }
}

static void transf_func(struct Parser_ASTNode *func_node,
                        const struct Parser_ASTNode *tmplt_node,
                        const struct Parser_TmpltArgVec *args)
{
    auto func = &func_node->func_decl;

    transf_type(&func->ret, tmplt_node, args);

    for (isize_t i = 0; i < func->params.len; ++i)
        transf_node(PARSER_GET_NODE(func->params.arr[i]), tmplt_node, args);

    for (isize_t i = 0; i < func->nodes.len; ++i)
        transf_node(func->nodes.arr[i], tmplt_node, args);
}

static void transf_class(struct Parser_ASTNode *class_node,
                         const struct Parser_ASTNode *tmplt_node,
                         const struct Parser_TmpltArgVec *args)
{
    auto class = &class_node->class_;

    for (isize_t i = 0; i < class->childs.len; ++i)
        transf_node(class->childs.arr[i], tmplt_node, args);
}

static void transf_node(struct Parser_ASTNode *node,
                        const struct Parser_ASTNode *tmplt_node,
                        const struct Parser_TmpltArgVec *args)
{
    switch (node->type) {
    case PARSER_ASTNODETYPE_EXPR:
        break;

    case PARSER_ASTNODETYPE_VAR_DECL:
        transf_var(node, tmplt_node, args);
        break;

    case PARSER_ASTNODETYPE_FUNC_DECL:
        transf_func(node, tmplt_node, args);
        break;

    case PARSER_ASTNODETYPE_CLASS:
        transf_class(node, tmplt_node, args);
        break;

    case PARSER_ASTNODETYPE_RETURN:
        break;

    case PARSER_ASTNODETYPE_TMPLT:
        CRASH("nested templates not supported yet");

    default:
        CRASH("can not instantiate this node type as part of a template");
    }
}

struct Parser_Type
Sema_instantiate_class_tmplt(struct Parser_ASTNode *tmplt_node,
                             const struct Parser_TmpltArgVec *args,
                             struct Parser_Allocators *allocs)
{
    auto tmplt = &tmplt_node->tmplt;
    assert(tmplt->child->type == PARSER_ASTNODETYPE_CLASS);

    struct Parser_TmpltInst inst = {};
    inst.args = Parser_copy_tmplt_argvec(args);
    gen_bumpmalloc(&allocs->scope, &inst.scope);
    gen_bumpmalloc(&allocs->ast, &inst.inst);

    *inst.scope = (struct Sema_Scope){.parent = tmplt->scope,
                                      .node = inst.inst,
                                      .type = SEMA_SCOPETYPE_TEMPLATE_INST};

    printf("copying class node\n");
    Parser_copy_node(inst.inst, tmplt->child, tmplt_node, inst.scope, allocs);
    printf("copy done\n");

    // the ident needs to be a class instead of a class template
    auto class = &inst.inst->class_;
    Sema_deref_identptr(&class->ident)->type = SEMA_IDENTTYPE_CLASS;

    transf_node(inst.inst, tmplt_node, &inst.args);

    Parser_log_node(inst.inst, stdout, 0);

    gen_dynpush(&tmplt->insts, inst);

    bool is_union = class->type == PARSER_CLASSTYPE_UNION;
    struct Parser_Type type = Parser_create_named_type(
        class->ident, is_union ? PARSER_TYPESPEC_UNION : PARSER_TYPESPEC_CLASS);

    return type;
}
