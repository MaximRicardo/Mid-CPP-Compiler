#include "sema/func.h"
#include "ints.h"
#include "parser/ast.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/type.h"
#include "sema/typecheck.h"

static bool decl_is_typedef(const struct midpar_VarDecl *decl)
{
    return decl->insts.arr[0]->type.squals.is_typedef;
}

static enum midsema_FuncConstexprSuitability
func_body_is_constexpr_suitable(const struct midpar_FuncDecl *self)
{
    if (self->quals.is_delete || self->quals.is_default)
        return MIDSEMA_FUNCCONSTEXPR_SUITABLE;

    // FIXME: add support for null statements, static_assert and using

    bool ret_found = false;
    for (mid_isize i = 0; i < self->nodes.len; ++i) {
        const struct midpar_ASTNode *node = self->nodes.arr[i];

        if (node->type == MIDPAR_ASTNODETYPE_RETURN) {
            if (midsema_func_is_ctor(self))
                return MIDSEMA_FUNCCONSTEXPR_RET_IN_CTOR;
            else if (ret_found)
                return MIDSEMA_FUNCCONSTEXPR_MULTIPLE_RET;
            ret_found = true;
        } else if (node->type == MIDPAR_ASTNODETYPE_VAR_DECL) {
            if (!decl_is_typedef(&node->var_decl))
                return MIDSEMA_FUNCCONSTEXPR_BAD_BODY;
        } else {
            return MIDSEMA_FUNCCONSTEXPR_BAD_BODY;
        }
    }

    return MIDSEMA_FUNCCONSTEXPR_SUITABLE;
}

enum midsema_FuncConstexprSuitability
midsema_func_constexpr_suitability(const struct midpar_FuncDecl *self)
{
    if (self->quals.is_virtual)
        return MIDSEMA_FUNCCONSTEXPR_VIRTUAL;

    if (!midsema_is_literal_type(&self->ret))
        return MIDSEMA_FUNCCONSTEXPR_NONLITERAL_RET;

    for (int i = 0; i < self->params.len; ++i) {
        const struct midpar_VarDeclInst *param =
            self->params.arr[i]->insts.arr[0];
        if (!midsema_is_literal_type(&param->type))
            return MIDSEMA_FUNCCONSTEXPR_NONLITERAL_PARAM;
    }

    return func_body_is_constexpr_suitable(self);
}

bool midsema_func_is_method(const struct midpar_FuncDecl *self)
{
    return midpar_func_parent(self)->type == MIDSEMA_SCOPETYPE_CLASS;
}

bool midsema_func_is_ctor(const struct midpar_FuncDecl *self)
{
    return self->is_tor && !self->is_dtor;
}

bool midsema_func_is_default_ctor(const struct midpar_FuncDecl *self)
{
    if (!midsema_func_is_ctor(self))
        return false;

    return self->params.len == 0;
}

bool midsema_func_is_copy_ctor(const struct midpar_FuncDecl *self)
{
    if (!midsema_func_is_ctor(self))
        return false;
    if (self->params.len != 1)
        return false;

    const struct midpar_ASTNode *class_node = MIDPAR_GET_PARENT(self);
    assert(class_node->type == MIDPAR_ASTNODETYPE_CLASS);
    const struct midpar_Class *class = &class_node->class_;

    // copy constructors have the signature
    // ClassName(const ClassName &)

    const struct midpar_Type *param = &self->params.arr[0]->insts.arr[0]->type;
    if (!midsema_type_is_class_or_union(param))
        return false;
    if (!param->lv_ref || !param->dquals.arr[0].is_const)
        return false;

    const struct midsema_Ident *ident = midsema_deref_identptr(&param->named);
    // class is guaranteed to be the definition of the class cuz there's a
    // function inside it
    return ident->def == MIDPAR_GET_NODE(class);
}

bool midsema_func_is_move_ctor(const struct midpar_FuncDecl *self)
{
    if (!midsema_func_is_ctor(self))
        return false;
    if (self->params.len != 1)
        return false;

    const struct midpar_ASTNode *class_node = MIDPAR_GET_PARENT(self);
    assert(class_node->type == MIDPAR_ASTNODETYPE_CLASS);
    const struct midpar_Class *class = &class_node->class_;

    // move constructors have the signature
    // ClassName(ClassName &&)

    const struct midpar_Type *param = &self->params.arr[0]->insts.arr[0]->type;
    if (!midsema_type_is_class_or_union(param))
        return false;
    if (!param->rv_ref)
        return false;

    const struct midsema_Ident *ident = midsema_deref_identptr(&param->named);
    // class is guaranteed to be the definition of the class cuz there's a
    // function inside it
    return ident->def == MIDPAR_GET_NODE(class);
}

bool midsema_func_takes_implicit_this(const struct midpar_FuncDecl *self,
                                      bool cnt_ctors)
{
    if (cnt_ctors && midsema_func_is_ctor(self))
        return true;
    return midsema_func_is_method(self) && !midsema_func_is_ctor(self) &&
           !self->ret.squals.is_static;
}

struct midpar_Type
midsema_implicit_this_type(const struct midpar_FuncDecl *self)
{
    const struct midsema_Scope *parent = midpar_func_parent(self);
    return midsema_node_type(parent->node, parent->parent);
}

bool midsema_func_is_main(const struct midpar_FuncDecl *self)
{
    return self->param_scope->parent->type == MIDSEMA_SCOPETYPE_ROOT &&
           !strcmp(self->name, "main");
}

bool midsema_is_user_provided(const struct midpar_FuncDecl *self)
{
    return !self->quals.is_default && !self->quals.is_delete;
}
