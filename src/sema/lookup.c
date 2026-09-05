#include "sema/lookup.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/class.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "sema/class.h"
#include "sema/func.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include "sema/typecheck.h"
#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_scope(struct midsema_ScopePVec *scopes,
                      struct midsema_Scope *scope)
{
    for (mid_isize i = 0; i < scopes->len; ++i) {
        if (scopes->arr[i] == scope)
            return;
    }

    midgen_dynpush(scopes, scope);
}

// adds the scope of the innermost enclosing namespace and any classes along the
// way
static void add_nmspace_scope(struct midsema_Scope *scope,
                              struct midsema_ScopePVec *scopes)
{
    bool stop = scope->type == MIDSEMA_SCOPETYPE_NAMESPACE ||
                scope->type == MIDSEMA_SCOPETYPE_ROOT;
    bool add = stop || scope->type == MIDSEMA_SCOPETYPE_CLASS;

    if (add)
        add_scope(scopes, scope);

    if (!stop) {
        assert(scope->parent);
        add_nmspace_scope(scope->parent, scopes);
    }
}

static void add_super_classes(const struct midpar_Class *self,
                              struct midsema_ScopePVec *scopes)
{
    for (mid_isize i = 0; i < self->supers.len; ++i) {
        auto super = self->supers.arr[i];
        assert(super);

        auto scope =
            midsema_deref_identptr(&super->ident)->class_info.def_scope;
        if (scope) {
            add_scope(scopes, scope);
            add_nmspace_scope(scope, scopes);
        }
        add_super_classes(super, scopes);
    }
}

static void get_assoc_scopes_class(const struct midpar_Expr *arg,
                                   struct midsema_ScopePVec *scopes)
{
    assert(midsema_is_typespec_named(arg->ret.spec));

    auto ident = midsema_deref_identptr(&arg->ret.named);
    auto node = ident->decl;
    assert(node);
    auto class_ = &node->class_;
    assert(class_);

    auto scope = midsema_deref_identptr(&class_->ident)->class_info.def_scope;
    if (scope) {
        add_scope(scopes, scope);
        add_nmspace_scope(scope, scopes);
    }
    add_super_classes(class_, scopes);
}

static void get_assoc_scopes(const struct midpar_Expr *args, mid_isize n_args,
                             struct midsema_ScopePVec *scopes)
{
    for (mid_isize i = 0; i < n_args; ++i) {
        if (args[i].ret.spec == MIDPAR_TYPESPEC_STRUCT ||
            args[i].ret.spec == MIDPAR_TYPESPEC_UNION)
            get_assoc_scopes_class(&args[i], scopes);
    }
}

static void add_class_ctors(const struct midpar_Class *class_,
                            struct midpar_FuncDeclPVec *funcs)
{
    auto ctors = midsema_class_ctors(class_);
    for (mid_isize j = 0; j < ctors.len; ++j)
        midgen_dynpush(funcs, ctors.arr[j]);
    midgen_dyndeinit(&ctors);
}

static void find_funcs_in_scope(const char *name,
                                const struct midsema_Scope *scope,
                                struct midpar_FuncDeclPVec *funcs)
{
    if (scope->type == MIDSEMA_SCOPETYPE_CLASS)
        add_class_ctors(&scope->node->class_, funcs);

    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (strcmp(ident->name, name))
            continue;

        if (ident->type == MIDSEMA_IDENTTYPE_FUNC) {
            midgen_dynpush(funcs, &ident->decl->func_decl);
        } else if (ident->type == MIDSEMA_IDENTTYPE_CLASS && ident->def) {
            add_class_ctors(&ident->def->class_, funcs);
        }
    }
}

static bool func_is_op_overload(const struct midpar_FuncDecl *decl,
                                enum midpar_ExprType op)
{
    return decl->is_op_overload && decl->op_overload == op &&
           !strcmp(decl->name, "operator");
}

static void find_op_overloads_in_scope(enum midpar_ExprType op,
                                       const struct midsema_Scope *scope,
                                       struct midpar_FuncDeclPVec *funcs)
{
    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (ident->type != MIDSEMA_IDENTTYPE_FUNC)
            continue;

        if (func_is_op_overload(&ident->decl->func_decl, op))
            midgen_dynpush(funcs, &ident->decl->func_decl);
    }
}

struct midpar_FuncDeclPVec
midsema_find_candidate_funcs(const char *name, const struct midpar_Expr *args,
                             mid_isize n_args, struct midsema_Scope *scope,
                             bool is_qualified)
{
    struct midsema_ScopePVec scopes = {};
    if (is_qualified) {
        midgen_dynpush(&scopes, scope);
    } else {
        add_nmspace_scope(scope, &scopes);
        get_assoc_scopes(args, n_args, &scopes);
    }

    struct midpar_FuncDeclPVec funcs = {};
    for (mid_isize i = 0; i < scopes.len; ++i)
        find_funcs_in_scope(name, scopes.arr[i], &funcs);

    midgen_dyndeinit(&scopes);
    return funcs;
}

struct midpar_FuncDecl *midsema_find_func(const char *name,
                                          const struct midpar_Expr *args,
                                          mid_isize n_args,
                                          struct midsema_Scope *scope,
                                          bool is_qualified)
{
    auto funcs =
        midsema_find_candidate_funcs(name, args, n_args, scope, is_qualified);

    struct midpar_FuncDecl *ret = NULL;
    if (funcs.len > 0)
        ret = midsema_best_viable_func(args, n_args, &funcs, NULL);

    midgen_dyndeinit(&funcs);
    return ret;
}

struct midpar_FuncDeclPVec
midsema_find_candidate_methods(const char *name, struct midsema_Scope *scope)
{
    assert(scope->type == MIDSEMA_SCOPETYPE_CLASS);

    struct midpar_FuncDeclPVec funcs = {};
    find_funcs_in_scope(name, scope, &funcs);

    return funcs;
}

struct midpar_FuncDecl *
midsema_find_method(const char *name, const struct midpar_Expr *args,
                    mid_isize n_args, struct midsema_Scope *scope,
                    const struct midpar_TypeDataQual *this_quals)
{
    assert(this_quals);
    assert(scope->type == MIDSEMA_SCOPETYPE_CLASS);

    auto funcs = midsema_find_candidate_methods(name, scope);

    struct midpar_FuncDecl *ret = NULL;
    if (funcs.len > 0)
        ret = midsema_best_viable_func(args, n_args, &funcs, this_quals);

    midgen_dyndeinit(&funcs);
    return ret;
}

struct midpar_FuncDecl *midsema_find_op_overload(enum midpar_ExprType op,
                                                 const struct midpar_Expr *args,
                                                 mid_isize n_args,
                                                 struct midsema_Scope *scope)
{
    struct midsema_ScopePVec scopes = {};
    add_nmspace_scope(scope, &scopes);
    get_assoc_scopes(args, n_args, &scopes);

    struct midpar_FuncDeclPVec funcs = {};
    for (mid_isize i = 0; i < scopes.len; ++i) {
        find_op_overloads_in_scope(op, scopes.arr[i], &funcs);
    }

    struct midpar_FuncDecl *ret = NULL;
    if (funcs.len > 0)
        ret = midsema_best_viable_func(args, n_args, &funcs, NULL);

    midgen_dyndeinit(&funcs);
    midgen_dyndeinit(&scopes);
    return ret;
}

static bool param_has_default(const struct midpar_FuncDecl *func,
                              mid_isize param)
{
    return midpar_func_ident(func)->func_info.default_args[param] != NULL;
}

static bool valid_this_arg(const struct midpar_FuncDecl *func,
                           const struct midpar_Expr *arg)
{
    assert(midsema_func_takes_implicit_this(func, false));

    if (arg->ret.spec != MIDPAR_TYPESPEC_STRUCT &&
        arg->ret.spec != MIDPAR_TYPESPEC_UNION)
        return false;
    if (midsema_n_indir(&arg->ret) > 0)
        return false;
    if (midsema_deref_identptr(&func->ret.named)->class_info.def_scope !=
        midpar_func_parent(func))
        return false;
    if (arg->ret.dquals.arr[0].is_const && !func->quals.is_const)
        return false;
    if (arg->ret.dquals.arr[0].is_volatile && !func->quals.is_volatile)
        return false;

    return true;
}

static bool func_params_viable(mid_isize n_args, bool implicit_this,
                               const struct midpar_FuncDecl *func)
{
    mid_isize n_params = func->params.len;

    bool skip_first =
        !implicit_this && midsema_func_takes_implicit_this(func, false);
    if (skip_first)
        --n_args;

    if (n_params == n_args)
        return true;
    else if (func->variadic && n_params < n_args)
        return true;
    else if (n_params > n_args && param_has_default(func, n_args))
        return true;
    return false;
}

bool midsema_is_func_viable(const struct midpar_Expr *args, mid_isize n_args,
                            const struct midpar_FuncDecl *func,
                            const struct midpar_TypeDataQual *this_quals)
{
    bool implicit_this = this_quals;

    if (!func_params_viable(n_args, implicit_this, func))
        return false;

    if (!implicit_this && midsema_func_takes_implicit_this(func, false) &&
        !valid_this_arg(func, &args[0]))
        return false;
    else if (implicit_this) {
        if (!midsema_func_takes_implicit_this(func, false))
            return false;
        if ((this_quals->is_const && !func->quals.is_const) ||
            (this_quals->is_volatile && !func->quals.is_volatile))
            return false;
    }

    mid_isize n = MID_MIN(n_args, func->params.len);
    for (mid_isize i = 0; i < n; ++i) {
        // if this is passed and the function implicitly takes this we can skip
        // the first arg
        mid_isize j =
            !implicit_this && midsema_func_takes_implicit_this(func, false)
                ? i + 1
                : i;
        if (!midsema_can_convert(&args[j].ret, args[j].valtype,
                                 &func->params.arr[i]->insts.arr[0]->type))
            return false;
    }

    return true;
}

struct midpar_FuncDeclPVec
midsema_viable_funcs(const struct midpar_Expr *args, mid_isize n_args,
                     const struct midpar_FuncDeclPVec *funcs,
                     const struct midpar_TypeDataQual *this_quals)
{
    struct midpar_FuncDeclPVec ret = {};

    for (mid_isize i = 0; i < funcs->len; ++i) {
        auto func = funcs->arr[i];
        if (midsema_is_func_viable(args, n_args, func, this_quals))
            midgen_dynpush(&ret, func);
    }

    return ret;
}

struct CmpViableFuncsInfo {
    const struct midpar_Expr *args;
    mid_isize n_args;
};

// returns 1 if a is better, -1 if b is better, and 0 if they're equal
static int compare_viable_funcs(const void *a_raw, const void *b_raw,
                                const void *info_raw)
{
    auto a = *(const struct midpar_FuncDecl **)a_raw;
    auto b = *(const struct midpar_FuncDecl **)b_raw;
    const struct CmpViableFuncsInfo *info = info_raw;

    bool has_better = false;
    for (mid_isize i = 0; i < info->n_args; ++i) {
        // keep in mind that variadic params have the lowest conversion rank
        bool in_a_variadic = i >= a->params.len;
        bool in_b_variadic = i >= b->params.len;
        if (in_a_variadic && in_b_variadic) {
            break;
        } else if (in_b_variadic) {
            return -1;
        } else if (in_a_variadic) {
            has_better = true;
            continue;
        }

        auto arg = &info->args[i];
        auto a_param = a->params.arr[i]->insts.arr[0];
        auto b_param = b->params.arr[i]->insts.arr[0];

        int a_rank = midsema_conversion_rank(&arg->ret, &a_param->type);
        int b_rank = midsema_conversion_rank(&arg->ret, &b_param->type);

        if (a_rank < b_rank)
            return -1;
        else if (a_rank > b_rank)
            has_better = true;
    }

    return has_better;
}

struct midpar_FuncDecl *
midsema_best_viable_func(const struct midpar_Expr *args, mid_isize n_args,
                         const struct midpar_FuncDeclPVec *funcs,
                         const struct midpar_TypeDataQual *this_quals)
{
    auto viable = midsema_viable_funcs(args, n_args, funcs, this_quals);
    if (viable.len == 0)
        return NULL;

    mid_qsort(viable.arr, viable.len, sizeof(*viable.arr), compare_viable_funcs,
              &(struct CmpViableFuncsInfo){.args = args, .n_args = n_args});

    auto ret = viable.arr[0];
    midgen_dyndeinit(&viable);
    return ret;
}
