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

static void transf_type(struct MidParser_Type *type,
                        const struct MidParser_ASTNode *tmplt_node,
                        const struct MidParser_TmpltArgVec *args)
{
    if (type->spec != MIDPARSER_TYPESPEC_TEMPLATED)
        return;

    auto tmplt = &tmplt_node->tmplt;

    const char *name = MidSema_deref_identptr(&type->named)->name;
    mid_isize idx = MidParser_tmplt_param_idx(tmplt, name);

    auto param = tmplt->params.arr[idx];
    assert(param->kind == MIDPARSER_TMPLTPARAM_TYPE);

    auto arg = &args->arr[idx];

    // the top most CV qualifier of the templated type is discarded
    for (mid_isize i = 1; i < arg->type.dquals.len; ++i) {
        MidGen_dynpush(&type->dquals, arg->type.dquals.arr[i]);
    }

    type->spec = arg->type.spec;

    if (arg->type.spec == MIDPARSER_TYPESPEC_FPTR) {
        type->fptr = Mid_malloc(sizeof(*type->fptr));
        *type->fptr = MidParser_copy_fptr_type(arg->type.fptr);
    } else if (arg->type.spec == MIDPARSER_TYPESPEC_ARRAY) {
        type->array = Mid_malloc(sizeof(*type->array));
        *type->array = MidParser_copy_array_type(arg->type.array);
    } else if (MidParser_is_typespec_named(arg->type.spec)) {
        type->named = arg->type.named;
    }
}

static void transf_node(struct MidParser_ASTNode *node,
                        const struct MidParser_ASTNode *tmplt_node,
                        const struct MidParser_TmpltArgVec *args);

static void transf_var_inst(struct MidParser_VarDeclInst *inst,
                            const struct MidParser_ASTNode *tmplt_node,
                            const struct MidParser_TmpltArgVec *args)
{
    transf_type(&inst->type, tmplt_node, args);
}

static void transf_var(struct MidParser_ASTNode *var_node,
                       const struct MidParser_ASTNode *tmplt_node,
                       const struct MidParser_TmpltArgVec *args)
{
    auto var = &var_node->var_decl;

    for (mid_isize i = 0; i < var->insts.len; ++i) {
        auto inst = var->insts.arr[i];
        transf_var_inst(inst, tmplt_node, args);
    }
}

static void transf_func(struct MidParser_ASTNode *func_node,
                        const struct MidParser_ASTNode *tmplt_node,
                        const struct MidParser_TmpltArgVec *args)
{
    auto func = &func_node->func_decl;

    transf_type(&func->ret, tmplt_node, args);

    for (mid_isize i = 0; i < func->params.len; ++i)
        transf_node(MIDPARSER_GET_NODE(func->params.arr[i]), tmplt_node, args);

    for (mid_isize i = 0; i < func->nodes.len; ++i)
        transf_node(func->nodes.arr[i], tmplt_node, args);
}

static void transf_class(struct MidParser_ASTNode *class_node,
                         const struct MidParser_ASTNode *tmplt_node,
                         const struct MidParser_TmpltArgVec *args)
{
    auto class = &class_node->class_;

    for (mid_isize i = 0; i < class->childs.len; ++i)
        transf_node(class->childs.arr[i], tmplt_node, args);
}

static void transf_node(struct MidParser_ASTNode *node,
                        const struct MidParser_ASTNode *tmplt_node,
                        const struct MidParser_TmpltArgVec *args)
{
    switch (node->type) {
    case MIDPARSER_ASTNODETYPE_EXPR:
        break;

    case MIDPARSER_ASTNODETYPE_VAR_DECL:
        transf_var(node, tmplt_node, args);
        break;

    case MIDPARSER_ASTNODETYPE_FUNC_DECL:
        transf_func(node, tmplt_node, args);
        break;

    case MIDPARSER_ASTNODETYPE_CLASS:
        transf_class(node, tmplt_node, args);
        break;

    case MIDPARSER_ASTNODETYPE_RETURN:
        break;

    case MIDPARSER_ASTNODETYPE_TMPLT:
        MID_CRASH("nested templates not supported yet");

    default:
        MID_CRASH("can not instantiate this node type as part of a template");
    }
}

struct MidParser_Type
MidSema_instantiate_class_tmplt(struct MidParser_ASTNode *tmplt_node,
                             const struct MidParser_TmpltArgVec *args,
                             struct MidParser_Allocators *allocs)
{
    auto tmplt = &tmplt_node->tmplt;
    assert(tmplt->child->type == MIDPARSER_ASTNODETYPE_CLASS);

    struct MidParser_TmpltInst inst = {};
    inst.args = MidParser_copy_tmplt_argvec(args);
    MidGen_bumpmalloc(&allocs->scope, &inst.scope);
    MidGen_bumpmalloc(&allocs->ast, &inst.inst);

    *inst.scope = (struct MidSema_Scope){.parent = tmplt->scope,
                                      .node = inst.inst,
                                      .type = MIDSEMA_SCOPETYPE_TEMPLATE_INST};

    printf("copying class node\n");
    MidParser_copy_node(inst.inst, tmplt->child, tmplt_node, inst.scope, allocs);
    printf("copy done\n");

    // the ident needs to be a class instead of a class template
    auto class = &inst.inst->class_;
    MidSema_deref_identptr(&class->ident)->type = MIDSEMA_IDENTTYPE_CLASS;

    transf_node(inst.inst, tmplt_node, &inst.args);

    MidParser_log_node(inst.inst, stdout, 0);

    MidGen_dynpush(&tmplt->insts, inst);

    bool is_union = class->type == MIDPARSER_CLASSTYPE_UNION;
    struct MidParser_Type type = MidParser_create_named_type(
        class->ident, is_union ? MIDPARSER_TYPESPEC_UNION : MIDPARSER_TYPESPEC_CLASS);

    return type;
}
