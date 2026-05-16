#include "lookup.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/class.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sort.h"
#include "type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// c++ is such a cooked language

static void add_scope(struct Sema_ScopePVec *scopes, struct Sema_Scope *scope)
{
    for (isize_t i = 0; i < scopes->len; ++i) {
        if (scopes->arr[i] == scope)
            return;
    }

    gen_dynpush(scopes, scope);
}

// adds the scope of the innermost enclosing namespace and any classes along the
// way
static void add_nmspace_scope(struct Sema_Scope *scope,
                              struct Sema_ScopePVec *scopes)
{
    bool stop = scope->type == SEMA_SCOPETYPE_NAMESPACE ||
                scope->type == SEMA_SCOPETYPE_ROOT;
    bool add = stop || scope->type == SEMA_SCOPETYPE_CLASS;

    if (add)
        add_scope(scopes, scope);

    if (!stop) {
        assert(scope->parent);
        add_nmspace_scope(scope->parent, scopes);
    }
}

static void add_super_classes(const struct Parser_Class *self,
                              struct Sema_ScopePVec *scopes)
{
    for (isize_t i = 0; i < self->supers.len; ++i) {
        auto super = self->supers.arr[i];
        assert(super);
        assert(super->type == PARSER_ASTNODETYPE_CLASS);

        auto scope = Parser_class_ident(&super->class_)->class_info.def_scope;
        if (scope) {
            add_scope(scopes, scope);
            add_nmspace_scope(scope, scopes);
        }
        add_super_classes(&super->class_, scopes);
    }
}

static void get_assoc_scopes_class(const struct Parser_Expr *arg,
                                   struct Sema_ScopePVec *scopes)
{
    assert(Parser_is_typespec_named(arg->ret.spec));

    auto ident = &arg->ret.named.parent->idents.arr[arg->ret.named.ident];
    auto node = ident->decl;
    assert(node);
    auto class_ = &node->class_;
    assert(class_);

    auto scope = Parser_class_ident(class_)->class_info.def_scope;
    if (scope) {
        add_scope(scopes, scope);
        add_nmspace_scope(scope, scopes);
    }
    add_super_classes(class_, scopes);
}

static void get_assoc_scopes(const struct Parser_Expr *args, isize_t n_args,
                             struct Sema_ScopePVec *scopes)
{
    for (isize_t i = 0; i < n_args; ++i) {
        if (args[i].ret.spec == PARSER_TYPESPEC_CLASS ||
            args[i].ret.spec == PARSER_TYPESPEC_UNION)
            get_assoc_scopes_class(&args[i], scopes);
    }
}

static void find_funcs_in_scope(const char *name,
                                const struct Sema_Scope *scope,
                                struct Parser_ASTNodePVec *nodes)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (ident->type != SEMA_IDENTTYPE_FUNC)
            continue;

        if (!strcmp(ident->name, name))
            gen_dynpush(nodes, ident->decl);
    }
}

static bool func_is_op_overload(const struct Parser_FuncDecl *decl,
                                enum Parser_ExprType op)
{
    return decl->is_op_overload && decl->op_overload == op &&
           !strcmp(decl->name, "operator");
}

static void find_op_overloads_in_scope(enum Parser_ExprType op,
                                       const struct Sema_Scope *scope,
                                       struct Parser_ASTNodePVec *nodes)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (ident->type != SEMA_IDENTTYPE_FUNC)
            continue;

        if (func_is_op_overload(&ident->decl->func_decl, op))
            gen_dynpush(nodes, ident->decl);
    }
}

struct Parser_ASTNode *
Sema_find_func(const char *name, const struct Parser_Expr *args, isize_t n_args,
               bool this_passed, struct Sema_Scope *scope, bool is_qualified)
{
    struct Sema_ScopePVec scopes = {};
    if (is_qualified) {
        gen_dynpush(&scopes, scope);
    } else {
        add_nmspace_scope(scope, &scopes);
        get_assoc_scopes(args, n_args, &scopes);
    }

    struct Parser_ASTNodePVec funcs;
    for (isize_t i = 0; i < scopes.len; ++i)
        find_funcs_in_scope(name, scopes.arr[i], &funcs);

    struct Parser_ASTNode *ret = NULL;
    if (funcs.len > 0)
        ret = Sema_best_viable_func(args, n_args, &funcs, this_passed);

    gen_dyndeinit(&funcs);
    gen_dyndeinit(&scopes);
    return ret;
}

struct Parser_ASTNode *Sema_find_op_overload(enum Parser_ExprType op,
                                             const struct Parser_Expr *args,
                                             isize_t n_args,
                                             struct Sema_Scope *scope)
{
    struct Sema_ScopePVec scopes = {};
    add_nmspace_scope(scope, &scopes);
    get_assoc_scopes(args, n_args, &scopes);

    struct Parser_ASTNodePVec funcs;
    for (isize_t i = 0; i < scopes.len; ++i) {
        find_op_overloads_in_scope(op, scopes.arr[i], &funcs);
    }

    struct Parser_ASTNode *ret = NULL;
    if (funcs.len > 0)
        ret = Sema_best_viable_func(args, n_args, &funcs, true);

    gen_dyndeinit(&funcs);
    gen_dyndeinit(&scopes);
    return ret;
}

static bool param_has_default(const struct Parser_FuncDecl *func, isize_t param)
{
    return Parser_func_ident(func)->func_info.default_args[param] != NULL;
}

// non-static methods take an implicit this parameter
static bool func_takes_this(const struct Parser_FuncDecl *func)
{
    return Parser_func_is_method(func) && !func->type.squals.is_static;
}

static bool valid_this_arg(const struct Parser_FuncDecl *func,
                           const struct Parser_Expr *arg)
{
    assert(func_takes_this(func));

    if (arg->ret.spec != PARSER_TYPESPEC_CLASS &&
        arg->ret.spec != PARSER_TYPESPEC_UNION)
        return false;
    if (Parser_n_indir(&arg->ret) > 0)
        return false;
    if (Parser_named_type_ident(&func->type.named)->class_info.def_scope !=
        Parser_func_parent(func))
        return false;
    if (arg->ret.dquals.arr[0].is_const && !func->quals.is_const)
        return false;
    if (arg->ret.dquals.arr[0].is_volatile && !func->quals.is_volatile)
        return false;

    return true;
}

static bool func_params_viable(isize_t n_args,
                               const struct Parser_FuncDecl *func,
                               bool this_passed)
{
    isize_t n_params = this_passed && func_takes_this(func)
                           ? func->params.len + 1
                           : func->params.len;

    if (n_params == n_args)
        return true;
    else if (func->variadic && n_params < n_args)
        return true;
    else if (n_params > n_args && param_has_default(func, n_args))
        return true;
    return false;
}

bool Sema_is_func_viable(const struct Parser_Expr *args, isize_t n_args,
                         const struct Parser_FuncDecl *func, bool this_passed)
{
    if (!func_params_viable(n_args, func, this_passed))
        return false;

    if (func_takes_this(func) && !valid_this_arg(func, &args[0]))
        return false;

    isize_t n = MIN(n_args, func->params.len);
    for (isize_t i = 0; i < n; ++i) {
        // if this is passed and the function implicitly takes this we can skip
        // the first arg
        isize_t j = this_passed && func_takes_this(func) ? i + 1 : i;
        if (!Sema_can_convert(&args[j].ret, args[j].valtype,
                              &func->params.arr[i]->var_decl.type))
            return false;
    }

    return true;
}

struct Parser_ASTNodePVec
Sema_viable_funcs(const struct Parser_Expr *args, isize_t n_args,
                  const struct Parser_ASTNodePVec *funcs, bool this_passed)
{
    struct Parser_ASTNodePVec ret = {};

    for (isize_t i = 0; i < funcs->len; ++i) {
        auto func = funcs->arr[i];
        if (Sema_is_func_viable(args, n_args, &func->func_decl, this_passed))
            gen_dynpush(&ret, func);
    }

    return ret;
}

struct CmpViableFuncsInfo {
    const struct Parser_Expr *args;
    isize_t n_args;
};

// returns 1 if a is better, -1 if b is better, and 0 if they're equal
static int compare_viable_funcs(const void *a_raw, const void *b_raw,
                                const void *info_raw)
{
    auto a = *(const struct Parser_ASTNode **)a_raw;
    auto b = *(const struct Parser_ASTNode **)b_raw;
    const struct CmpViableFuncsInfo *info = info_raw;

    bool has_better = false;
    for (isize_t i = 0; i < info->n_args; ++i) {
        // keep in mind that variadic params have the lowest conversion rank
        bool in_a_variadic = i >= a->func_decl.params.len;
        bool in_b_variadic = i >= b->func_decl.params.len;
        if (in_a_variadic && in_b_variadic) {
            break;
        } else if (in_b_variadic) {
            return -1;
        } else if (in_a_variadic) {
            has_better = true;
            continue;
        }

        auto arg = &info->args[i];
        auto a_param = a->func_decl.params.arr[i];
        auto b_param = b->func_decl.params.arr[i];

        int a_rank = Sema_conversion_rank(&arg->ret, &a_param->var_decl.type);
        int b_rank = Sema_conversion_rank(&arg->ret, &b_param->var_decl.type);

        if (a_rank < b_rank)
            return -1;
        else if (a_rank > b_rank)
            has_better = true;
    }

    return has_better;
}

struct Parser_ASTNode *
Sema_best_viable_func(const struct Parser_Expr *args, isize_t n_args,
                      const struct Parser_ASTNodePVec *funcs, bool this_passed)
{
    auto viable = Sema_viable_funcs(args, n_args, funcs, this_passed);
    if (viable.len == 0)
        return NULL;

    better_qsort(viable.arr, viable.len, sizeof(*viable.arr),
                 compare_viable_funcs,
                 &(struct CmpViableFuncsInfo){.args = args, .n_args = n_args});

    auto ret = viable.arr[0];
    gen_dyndeinit(&viable);
    return ret;
}
