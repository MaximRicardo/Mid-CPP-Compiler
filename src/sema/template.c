#include "sema/template.h"
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
#include "sema/type.h"
#include <stdio.h>

static void transf_type(struct midpar_Type *type,
                        const struct midpar_ASTNode *tmplt_node,
                        const struct midpar_TmpltArgVec *args)
{
    if (type->spec != MIDPAR_TYPESPEC_TEMPLATED)
        return;

    auto tmplt = &tmplt_node->tmplt;

    const char *name = midsema_deref_identptr(&type->named)->name;
    mid_isize idx = midpar_tmplt_param_idx(tmplt, name);

    auto param = tmplt->params.arr[idx];
    assert(param->kind == MIDPAR_TMPLTPARAM_TYPE);

    auto arg = &args->arr[idx];

    // the top most CV qualifier of the templated type is discarded
    for (mid_isize i = 1; i < arg->type.dquals.len; ++i) {
        midgen_dynpush(&type->dquals, arg->type.dquals.arr[i]);
    }

    type->spec = arg->type.spec;

    if (arg->type.spec == MIDPAR_TYPESPEC_FPTR) {
        type->fptr = mid_malloc(sizeof(*type->fptr));
        *type->fptr = midpar_copy_fptr_type(arg->type.fptr);
    } else if (arg->type.spec == MIDPAR_TYPESPEC_ARRAY) {
        type->array = mid_malloc(sizeof(*type->array));
        *type->array = midpar_copy_array_type(arg->type.array);
    } else if (midsema_is_typespec_named(arg->type.spec)) {
        type->named = arg->type.named;
    }
}

static void transf_node(struct midpar_ASTNode *node,
                        const struct midpar_ASTNode *tmplt_node,
                        const struct midpar_TmpltArgVec *args);

static void transf_var_inst(struct midpar_VarDeclInst *inst,
                            const struct midpar_ASTNode *tmplt_node,
                            const struct midpar_TmpltArgVec *args)
{
    transf_type(&inst->type, tmplt_node, args);
}

static void transf_var(struct midpar_ASTNode *var_node,
                       const struct midpar_ASTNode *tmplt_node,
                       const struct midpar_TmpltArgVec *args)
{
    auto var = &var_node->var_decl;

    for (mid_isize i = 0; i < var->insts.len; ++i) {
        auto inst = var->insts.arr[i];
        transf_var_inst(inst, tmplt_node, args);
    }
}

static void transf_func(struct midpar_ASTNode *func_node,
                        const struct midpar_ASTNode *tmplt_node,
                        const struct midpar_TmpltArgVec *args)
{
    auto func = &func_node->func_decl;

    transf_type(&func->ret, tmplt_node, args);

    for (mid_isize i = 0; i < func->params.len; ++i)
        transf_node(MIDPAR_GET_NODE(func->params.arr[i]), tmplt_node, args);

    for (mid_isize i = 0; i < func->nodes.len; ++i)
        transf_node(func->nodes.arr[i], tmplt_node, args);
}

static void transf_class(struct midpar_ASTNode *class_node,
                         const struct midpar_ASTNode *tmplt_node,
                         const struct midpar_TmpltArgVec *args)
{
    auto class = &class_node->class_;

    for (mid_isize i = 0; i < class->childs.len; ++i)
        transf_node(class->childs.arr[i], tmplt_node, args);
}

static void transf_node(struct midpar_ASTNode *node,
                        const struct midpar_ASTNode *tmplt_node,
                        const struct midpar_TmpltArgVec *args)
{
    switch (node->type) {
    case MIDPAR_ASTNODETYPE_EXPR:
        break;

    case MIDPAR_ASTNODETYPE_VAR_DECL:
        transf_var(node, tmplt_node, args);
        break;

    case MIDPAR_ASTNODETYPE_FUNC_DECL:
        transf_func(node, tmplt_node, args);
        break;

    case MIDPAR_ASTNODETYPE_CLASS:
        transf_class(node, tmplt_node, args);
        break;

    case MIDPAR_ASTNODETYPE_RETURN:
        break;

    case MIDPAR_ASTNODETYPE_TMPLT:
        MID_CRASH("nested templates not supported yet");

    default:
        MID_CRASH("can not instantiate this node type as part of a template");
    }
}

struct midpar_Type
midsema_instantiate_class_tmplt(struct midpar_ASTNode *tmplt_node,
                                const struct midpar_TmpltArgVec *args,
                                struct midpar_Allocators *allocs)
{
    auto tmplt = &tmplt_node->tmplt;
    assert(tmplt->child->type == MIDPAR_ASTNODETYPE_CLASS);

    struct midpar_TmpltInst inst = {};
    inst.args = midpar_copy_tmplt_argvec(args);
    midgen_bumpmalloc(&allocs->scope, &inst.scope);
    midgen_bumpmalloc(&allocs->ast, &inst.inst);

    *inst.scope =
        (struct midsema_Scope){.parent = tmplt->scope,
                               .node = inst.inst,
                               .type = MIDSEMA_SCOPETYPE_TEMPLATE_INST};

    printf("copying class node\n");
    midpar_copy_node(inst.inst, tmplt->child, tmplt_node, inst.scope, allocs);
    printf("copy done\n");

    // the ident needs to be a class instead of a class template
    auto class = &inst.inst->class_;
    midsema_deref_identptr(&class->ident)->type = MIDSEMA_IDENTTYPE_CLASS;

    transf_node(inst.inst, tmplt_node, &inst.args);

    midpar_log_node(inst.inst, stdout, 0);

    midgen_dynpush(&tmplt->insts, inst);

    bool is_union = class->type == MIDPAR_CLASSTYPE_UNION;
    struct midpar_Type type = midpar_create_named_type(
        class->ident, is_union ? MIDPAR_TYPESPEC_UNION : MIDPAR_TYPESPEC_CLASS);

    return type;
}
