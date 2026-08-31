#include "sema/class.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/func_decl.h"
#include "parser/var_decl.h"
#include "sema/class_lit.h"
#include "sema/expr_eval.h"
#include "sema/func.h"
#include "sema/ident.h"
#include "sema/type.h"
#include "sema/var.h"
#include <string.h>

static const struct midpar_ASTNode *
get_base_child_const(const struct midpar_Class *parent,
                     const struct midpar_ASTNode *child)
{
    if (child->type == MIDPAR_ASTNODETYPE_VAR_DECL_INST) {
        child = child->parent;
        assert(child->type == MIDPAR_ASTNODETYPE_VAR_DECL);
        assert(child->parent->type == MIDPAR_ASTNODETYPE_CLASS);

        if (child->parent != MIDPAR_GET_NODE(parent)) {
            // child is part of a nested class with a built in var decl like
            // this:
            // class Parent {
            //     class Nested {
            //         ...
            //     } child;
            // };
            child = child->parent;
            assert(child->parent == MIDPAR_GET_NODE(parent));
        }
    }

    return child;
}

bool midsema_is_field_pub(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child)
{
    child = get_base_child_const(self, child);

    for (mid_isize i = 0; i < self->pub_childs.len; ++i) {
        if (child == self->pub_childs.arr[i])
            return true;
    }

    return false;
}

bool midsema_is_field_priv(const struct midpar_Class *self,
                           const struct midpar_ASTNode *child)
{
    child = get_base_child_const(self, child);

    for (mid_isize i = 0; i < self->priv_childs.len; ++i) {
        if (child == self->priv_childs.arr[i])
            return true;
    }

    return false;
}

bool midsema_is_field_prot(const struct midpar_Class *self,
                           const struct midpar_ASTNode *child)
{
    child = get_base_child_const(self, child);

    for (mid_isize i = 0; i < self->prot_childs.len; ++i) {
        if (child == self->prot_childs.arr[i])
            return true;
    }

    return false;
}

enum midpar_ClassAccess midsema_field_access(const struct midpar_Class *self,
                                             const struct midpar_ASTNode *child)
{
    if (midsema_is_field_pub(self, child))
        return MIDPAR_CLASSACCESS_PUBLIC;
    else if (midsema_is_field_priv(self, child))
        return MIDPAR_CLASSACCESS_PRIVATE;
    else if (midsema_is_field_prot(self, child))
        return MIDPAR_CLASSACCESS_PROTECTED;
    else
        MID_CRASH("child isn't in class");
}

struct midpar_ASTNode *midsema_find_field(const struct midpar_Class *self,
                                          const char *name)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type == MIDPAR_ASTNODETYPE_VAR_DECL) {
            struct midpar_VarDeclInst *inst =
                midpar_decl_inst_of_name(&child->var_decl, name);
            if (inst)
                return MIDPAR_GET_NODE(inst);
        } else if (child->type == MIDPAR_ASTNODETYPE_CLASS &&
                   child->class_.var) {
            struct midpar_VarDeclInst *inst =
                midpar_decl_inst_of_name(child->class_.var, name);
            if (inst)
                return MIDPAR_GET_NODE(inst);
        } else if (child->type == MIDPAR_ASTNODETYPE_FUNC_DECL) {
            if (!strcmp(child->func_decl.name, name))
                return child;
        }
    }

    return nullptr;
}

static void add_if_nonstatic_dfield(struct midpar_VarDeclInstPVec *dfields,
                                    struct midpar_ASTNode *field)
{
    struct midpar_VarDecl *decl = nullptr;
    if (field->type == MIDPAR_ASTNODETYPE_VAR_DECL)
        decl = &field->var_decl;
    else if (field->type == MIDPAR_ASTNODETYPE_CLASS)
        decl = field->class_.var;

    if (!decl || decl->insts.arr[0]->type.squals.is_static)
        return;

    for (mid_isize i = 0; i < decl->insts.len; ++i)
        midgen_dynpush(dfields, decl->insts.arr[i]);
}

struct midpar_VarDeclInstPVec
midsema_nonstatic_dfields(const struct midpar_Class *self)
{
    struct midpar_VarDeclInstPVec dfields = {};

    for (mid_isize i = 0; i < self->childs.len; ++i) {
        add_if_nonstatic_dfield(&dfields, self->childs.arr[i]);
    }

    return dfields;
}

struct midpar_FuncDecl *
midsema_class_default_ctor(const struct midpar_Class *self)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        struct midpar_ASTNode *child = self->childs.arr[i];
        if (child->type != MIDPAR_ASTNODETYPE_FUNC_DECL)
            continue;
        struct midpar_FuncDecl *func = &child->func_decl;

        if (midsema_func_is_default_ctor(func))
            return func;
    }

    return NULL;
}

struct midpar_FuncDeclPVec midsema_class_ctors(const struct midpar_Class *self)
{
    struct midpar_FuncDeclPVec ret = {};

    for (mid_isize i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type != MIDPAR_ASTNODETYPE_FUNC_DECL)
            continue;
        if (!child->func_decl.is_tor || child->func_decl.is_dtor)
            continue;

        midgen_dynpush(&ret, &child->func_decl);
    }

    return ret;
}

struct midpar_FuncDecl *midsema_class_dtor(const struct midpar_Class *self)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        struct midpar_ASTNode *child = self->childs.arr[i];
        if (child->type != MIDPAR_ASTNODETYPE_FUNC_DECL)
            continue;

        struct midpar_FuncDecl *func = &child->func_decl;
        if (func->is_dtor)
            return func;
    }

    return NULL;
}

bool midsema_has_explicit_ctors(const struct midpar_Class *self)
{
    struct midpar_FuncDeclPVec ctors = midsema_class_ctors(self);

    bool ret = false;
    for (mid_isize i = 0; i < ctors.len; ++i) {
        if (ctors.arr[i]->quals.is_explicit) {
            ret = true;
            break;
        }
    }

    midgen_dyndeinit(&ctors);
    return ret;
}

bool midsema_has_user_provided_ctors(const struct midpar_Class *self)
{
    struct midpar_FuncDeclPVec ctors = midsema_class_ctors(self);

    bool ret = false;
    for (mid_isize i = 0; i < ctors.len; ++i) {
        if (midsema_is_user_provided(ctors.arr[i])) {
            ret = true;
            break;
        }
    }

    midgen_dyndeinit(&ctors);
    return ret;
}

bool midsema_has_user_provided_dtor(const struct midpar_Class *self)
{
    const struct midpar_FuncDecl *dtor = midsema_class_dtor(self);
    if (!dtor)
        return false;
    else
        return midsema_is_user_provided(dtor);
}

static bool decl_is_nonstatic(const struct midpar_VarDecl *decl)
{
    return !decl->insts.arr[0]->type.squals.is_static;
}

bool midsema_has_trivial_dtor(const struct midpar_Class *self)
{
    const struct midpar_FuncDecl *dtor = midsema_class_dtor(self);
    if (!dtor)
        return true;

    if (midsema_is_user_provided(dtor))
        return false;
    if (dtor->quals.is_virtual)
        return false;

    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (!midsema_has_trivial_dtor(self->supers.arr[i]))
            return false;
    }

    // every non-static data member must have a trivial destructor as well
    for (mid_isize child_i = 0; child_i < self->childs.len; ++child_i) {
        const struct midpar_ASTNode *child = self->childs.arr[child_i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        const struct midpar_VarDecl *decl = &child->var_decl;
        if (decl_is_nonstatic(decl))
            continue;

        for (mid_isize inst_i = 0; inst_i < decl->insts.len; ++inst_i) {
            const struct midpar_VarDeclInst *inst = decl->insts.arr[inst_i];

            if (!midsema_type_has_trivial_dtor(&inst->type))
                return false;
        }
    }

    return true;
}

static bool
all_fields_are_constexpr_in_class_inited(const struct midpar_Class *self)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        const struct midpar_ASTNode *child = self->childs.arr[i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        if (decl_is_nonstatic(&child->var_decl))
            continue;

        const struct midpar_VarDecl *decl = &child->var_decl;
        for (int i = 0; i < decl->insts.len; ++i) {
            if (!midsema_var_inst_is_constexpr_inited(decl->insts.arr[i]))
                return false;
        }
    }

    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (!all_fields_are_constexpr_in_class_inited(self->supers.arr[i]))
            return false;
    }

    return true;
}

bool midsema_class_is_constexpr_default_constructible(
    const struct midpar_Class *self)
{
    if (midsema_class_has_constexpr_default_ctor(self))
        return true;

    if (!midsema_class_is_literal(self))
        return false;

    if (!all_fields_are_constexpr_in_class_inited(self))
        return false;

    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (!midsema_class_is_constexpr_default_constructible(
                self->supers.arr[i]))
            return false;
    }

    return true;
}

static bool
union_has_nonvolatile_literal_variant(const struct midpar_Class *self)
{
    assert(self->type == MIDPAR_CLASSTYPE_UNION);

    for (mid_isize child_i = 0; child_i < self->childs.len; ++child_i) {
        const struct midpar_ASTNode *child = self->childs.arr[child_i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        const struct midpar_VarDecl *decl = &child->var_decl;

        for (mid_isize inst_i = 0; inst_i < decl->insts.len; ++inst_i) {
            const struct midpar_VarDeclInst *inst = decl->insts.arr[inst_i];
            if (inst->type.dquals.arr[0].is_volatile)
                continue;

            if (midsema_type_is_literal(&inst->type))
                return true;
        }
    }

    return false;
}

static bool is_literal_nonunion_aggr_case(const struct midpar_Class *self)
{
    // the class is a literal if each of its anonymous union members:
    //    has no variant member
    // or
    //    has at least one variant member of non-volatile literal type

    for (mid_isize i = 0; i < self->childs.len; ++i) {
        const struct midpar_ASTNode *child = self->childs.arr[i];
        if (child->type != MIDPAR_ASTNODETYPE_CLASS)
            continue;
        const struct midpar_Class *union_ = &child->class_;
        if (union_->type != MIDPAR_CLASSTYPE_UNION)
            continue;

        if (union_->name)
            continue;

        if (midsema_union_has_variant_member(union_))
            return false;
        if (!union_has_nonvolatile_literal_variant(union_))
            return false;
    }

    return true;
}

static bool is_literal_default_case(const struct midpar_Class *self)
{
    // self must have at least one constexpr ctor that is not a copy or move
    // ctor

    // a constexpr default ctor can be implicitly generated
    if (all_fields_are_constexpr_in_class_inited(self))
        return true;

    struct midpar_FuncDeclPVec ctors = midsema_class_ctors(self);

    bool res = false;
    for (mid_isize i = 0; i < ctors.len; ++i) {
        const struct midpar_FuncDecl *ctor = ctors.arr[i];
        if (!ctor->quals.is_constexpr)
            continue;

        if (!midsema_func_is_copy_ctor(ctor) &&
            !midsema_func_is_move_ctor(ctor)) {
            res = true;
            break;
        }
    }

    midgen_dyndeinit(&ctors);
    return res;
}

bool midsema_class_is_literal(const struct midpar_Class *self)
{
    if (!midsema_has_trivial_dtor(self))
        return false;

    bool is_union = self->type == MIDPAR_CLASSTYPE_UNION;

    // every non-static non-variant data member must be of non-volatile
    // literal type
    for (mid_isize child_i = 0; !is_union && child_i < self->childs.len;
         ++child_i) {
        const struct midpar_ASTNode *child = self->childs.arr[child_i];
        if (child->type == MIDPAR_ASTNODETYPE_CLASS) {
            if (!child->class_.var)
                continue;
            child = MIDPAR_GET_NODE(child->class_.var);
        }

        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        const struct midpar_VarDecl *decl = &child->var_decl;

        for (mid_isize inst_i = 0; inst_i < decl->insts.len; ++inst_i) {
            const struct midpar_VarDeclInst *inst = decl->insts.arr[inst_i];
            if (inst->type.dquals.arr[0].is_volatile)
                return false;
            else if (!midsema_type_is_literal(&inst->type))
                return false;
        }
    }

    // every base class also has to be a literal type
    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (!midsema_class_is_literal(self->supers.arr[i]))
            return false;
    }

    bool is_aggr = midsema_class_is_aggregate(self);

    if (is_union && is_aggr)
        return midsema_union_has_variant_member(self) ||
               union_has_nonvolatile_literal_variant(self);
    else if (is_aggr)
        return is_literal_nonunion_aggr_case(self);
    else
        return is_literal_default_case(self);
}

static bool
has_direct_nonstatic_priv_data_membs(const struct midpar_Class *self)
{
    for (mid_isize child_i = 0; child_i < self->priv_childs.len; ++child_i) {
        const struct midpar_ASTNode *child = self->priv_childs.arr[child_i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;

        if (decl_is_nonstatic(&child->var_decl))
            return true;
    }

    return false;
}

static bool
has_direct_nonstatic_prot_data_membs(const struct midpar_Class *self)
{
    for (mid_isize child_i = 0; child_i < self->prot_childs.len; ++child_i) {
        const struct midpar_ASTNode *child = self->prot_childs.arr[child_i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;

        if (decl_is_nonstatic(&child->var_decl))
            return true;
    }

    return false;
}

bool midsema_class_is_aggregate(const struct midpar_Class *self)
{
    if (midsema_has_user_provided_ctors(self))
        return false;
    if (midsema_has_inherited_ctors(self))
        return false;
    if (midsema_has_explicit_ctors(self))
        return false;

    if (self->supers.len > 0)
        return false;

    if (has_direct_nonstatic_priv_data_membs(self))
        return false;
    if (has_direct_nonstatic_prot_data_membs(self))
        return false;

    if (midsema_has_virt_methods(self))
        return false;

    if (midsema_has_default_memb_initializers(self))
        return false;

    return true;
}

bool midsema_has_inherited_ctors(const struct midpar_Class *self)
{
    // TODO: implement this when i add inheriting constructors in the first
    //       place
    (void)self;
    return false;
}

bool midsema_has_virt_methods(const struct midpar_Class *self)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        const struct midpar_ASTNode *child = self->childs.arr[i];
        if (child->type != MIDPAR_ASTNODETYPE_FUNC_DECL)
            continue;
        const struct midpar_FuncDecl *func = &child->func_decl;

        if (func->quals.is_virtual)
            return true;
    }

    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (midsema_has_virt_methods(self->supers.arr[i]))
            return true;
    }

    return false;
}

bool decl_has_initializer(const struct midpar_VarDecl *decl)
{
    for (mid_isize i = 0; i < decl->insts.len; ++i) {
        const struct midpar_VarDeclInst *inst = decl->insts.arr[i];
        if (midsema_var_inst_is_inited(inst))
            return true;
    }

    return false;
}

static bool default_ctor_has_memb_inits(const struct midpar_Class *self)
{
    const struct midpar_FuncDecl *ctor = midsema_class_default_ctor(self);
    if (!ctor)
        return false;

    return ctor->memb_inits.len > 0;
}

bool midsema_has_default_memb_initializers(const struct midpar_Class *self)
{
    if (default_ctor_has_memb_inits(self))
        return true;

    for (mid_isize i = 0; i < self->childs.len; ++i) {
        const struct midpar_ASTNode *child = self->childs.arr[i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        if (decl_is_nonstatic(&child->var_decl))
            continue;

        if (decl_has_initializer(&child->var_decl))
            return true;
    }

    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (midsema_has_default_memb_initializers(self->supers.arr[i]))
            return true;
    }

    return false;
}

bool midsema_union_has_variant_member(const struct midpar_Class *self)
{
    assert(self->type == MIDPAR_CLASSTYPE_UNION);

    for (mid_isize child_i = 0; child_i < self->childs.len; ++child_i) {
        const struct midpar_ASTNode *child = self->childs.arr[child_i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        const struct midpar_VarDecl *decl = &child->var_decl;
        if (!decl_is_nonstatic(decl))
            continue;

        for (mid_isize inst_i = 0; inst_i < decl->insts.len; ++inst_i) {
            const struct midpar_VarDeclInst *inst = decl->insts.arr[inst_i];

            if (midsema_type_has_trivial_default_ctor(&inst->type))
                return true;
        }
    }

    return false;
}

bool midsema_has_default_ctor(const struct midpar_Class *self)
{
    const struct midpar_FuncDecl *ctor = midsema_class_default_ctor(self);
    return ctor && !ctor->quals.is_delete;
}

static bool class_type_has_trivial_default_ctor(const struct midpar_Type *type)
{
    assert(midsema_type_is_class_or_union(type));

    const struct midsema_Ident *ident = midsema_deref_identptr(&type->named);
    assert(ident->def);
    assert(ident->def->type == MIDPAR_ASTNODETYPE_CLASS);

    return midsema_has_trivial_default_ctor(&ident->def->class_);
}

bool midsema_class_is_trivially_constructible(const struct midpar_Class *self)
{
    // TODO: make sure there are no virtual base classes once i add those

    if (midsema_has_virt_methods(self))
        return false;
    if (midsema_has_default_memb_initializers(self))
        return false;

    for (mid_isize i = 0; i < self->supers.len; ++i) {
        if (!midsema_has_trivial_default_ctor(self->supers.arr[i]))
            return false;
    }

    if (self->type == MIDPAR_CLASSTYPE_UNION)
        return true;

    // every non-static non-variant data member of self that is of class type or
    // an array thereof must have a trivial default ctor
    for (mid_isize child_i = 0; child_i < self->childs.len; ++child_i) {
        const struct midpar_ASTNode *child = self->childs.arr[child_i];
        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;
        const struct midpar_VarDecl *decl = &child->var_decl;
        if (!decl_is_nonstatic(decl))
            continue;

        for (mid_isize inst_i = 0; inst_i < self->childs.len; ++inst_i) {
            const struct midpar_VarDeclInst *inst = decl->insts.arr[inst_i];

            if (midsema_type_is_class_or_union(&inst->type) &&
                !class_type_has_trivial_default_ctor(&inst->type))
                return false;
            else if (midsema_type_is_array(&inst->type) &&
                     !class_type_has_trivial_default_ctor(
                         &inst->type.array->elem))
                return false;
        }
    }

    return true;
}

bool midsema_is_ctor_trivial(const struct midpar_FuncDecl *ctor)
{
    assert(midsema_func_is_ctor(ctor));

    if (!ctor->quals.is_default)
        return false;

    const struct midpar_ASTNode *class_node = MIDPAR_GET_PARENT(ctor);
    assert(class_node->type == MIDPAR_ASTNODETYPE_CLASS);

    return midsema_class_is_trivially_constructible(&class_node->class_);
}

bool midsema_has_trivial_default_ctor(const struct midpar_Class *self)
{
    if (!midsema_has_default_ctor(self))
        return false;

    return midsema_class_is_trivially_constructible(self);
}

static bool default_init_class_inst(const struct midpar_VarDeclInst *inst,
                                    struct midlit_TaggedValue *out_val)
{
    struct midpar_Class *inst_class =
        &midsema_deref_identptr(&inst->type.named)->def->class_;

    assert(inst_class->type != MIDPAR_CLASSTYPE_UNION);
    out_val->kind = MIDLIT_VALUE_STRUCT;
    return midsema_constexpr_default_init_struct(inst_class,
                                                 &out_val->v.struct_);
}

static bool get_default_memb_init_value(const struct midpar_Class *self,
                                        const char *name,
                                        struct midlit_TaggedValue *out_val)
{
    const struct midpar_ASTNode *field = midsema_find_field(self, name);
    assert(field);

    if (field->type != MIDPAR_ASTNODETYPE_VAR_DECL_INST)
        return false;

    const struct midpar_VarDeclInst *inst = &field->var_inst;

    // TODO: add support for calling constructors for a field's default value
    if (inst->has_ctor) {
        MID_CRASH("calling constructors for a field's default value isn't "
                  "supported yet");
    } else if (inst->init.expr) {
        if (!inst->init.expr->constant)
            return false;
        *out_val = midsema_eval_expr(inst->init.expr, midpar_class_scope(self));
    } else {
        // primitive types without an explicit initalizer are undefined
        if (!midsema_type_is_class_or_union(&inst->type))
            return false;

        return default_init_class_inst(inst, out_val);
    }

    return true;
}

static bool get_ctor_init_value(const struct midpar_Class *self,
                                const struct midpar_CtorMemberInit *init,
                                struct midlit_TaggedValue *out_val)
{
    const struct midpar_ASTNode *field = midsema_find_field(self, init->name);
    const struct midpar_VarDeclInst *inst = &field->var_inst;

    if (init->n_args == 0)
        return midsema_constexpr_default_init_type(&inst->type, out_val);

    MID_CRASH(
        "calling constructors for a field's default value isn't supported yet");
}

static bool get_default_ctor_init_list_value(const struct midpar_Class *self,
                                             const char *name,
                                             struct midlit_TaggedValue *out_val)
{
    const struct midpar_FuncDecl *ctor = midsema_class_default_ctor(self);
    if (!ctor || !ctor->quals.is_constexpr)
        return false;

    for (mid_isize i = 0; i < ctor->memb_inits.len; ++i) {
        const struct midpar_CtorMemberInit *init = ctor->memb_inits.arr[i];

        if (!strcmp(init->name, name))
            return get_ctor_init_value(self, init, out_val);
    }

    return false;
}

bool midsema_field_default_value(const struct midpar_Class *self,
                                 const char *name,
                                 struct midlit_TaggedValue *out_val)
{
    if (get_default_ctor_init_list_value(self, name, out_val))
        return true;

    return get_default_memb_init_value(self, name, out_val);
}

bool midsema_class_has_constexpr_default_ctor(const struct midpar_Class *self)
{
    const struct midpar_FuncDecl *ctor = midsema_class_default_ctor(self);
    return ctor && ctor->quals.is_constexpr;
}

bool midsema_field_is_nonstatic_data_memb(const struct midpar_Class *self,
                                          const char *name)
{
    const struct midpar_ASTNode *node = midsema_find_field(self, name);
    if (!node || node->type != MIDPAR_ASTNODETYPE_VAR_DECL_INST)
        return false;

    return !node->var_inst.type.squals.is_static;
}
