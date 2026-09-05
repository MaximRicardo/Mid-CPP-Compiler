#include "sema/typecheck.h"
#include "cmd.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/class.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/func_decl.h"
#include "parser/template.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/class.h"
#include "sema/expr.h"
#include "sema/expr_eval.h"
#include "sema/func.h"
#include "sema/ident.h"
#include "sema/lookup.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// root - is expr the root of the expression tree
static void typecheck_expr(struct midpar_Expr *expr,
                           struct midsema_Scope *scope,
                           struct mid_DiagVec *diags, bool root);

bool midsema_node_creates_new_type(const struct midpar_ASTNode *node)
{
    switch (node->type) {
    case MIDPAR_ASTNODETYPE_CLASS:
        return true;

    case MIDPAR_ASTNODETYPE_ENUM:
        return true;

    case MIDPAR_ASTNODETYPE_VAR_DECL_INST:
        return node->var_inst.type.squals.is_typedef;

    case MIDPAR_ASTNODETYPE_TMPLT_PARAM:
        return node->tmplt_param.kind == MIDPAR_TMPLTPARAM_TYPE;

    default:
        return false;
    }
}

static struct midpar_Type class_node_type(const struct midpar_ASTNode *node)
{
    auto class_ = &node->class_;

    struct midpar_Type ret = {};
    ret.spec = class_->type == MIDPAR_CLASSTYPE_UNION ? MIDPAR_TYPESPEC_UNION
                                                      : MIDPAR_TYPESPEC_CLASS;
    ret.named = class_->ident;
    midgen_dynpush(&ret.dquals, (struct midpar_TypeDataQual){});

    return ret;
}

struct midpar_Type midsema_node_type(const struct midpar_ASTNode *node,
                                     struct midsema_Scope *scope)
{
    if (node->type == MIDPAR_ASTNODETYPE_VAR_DECL_INST) {
        return midpar_copy_type(&node->var_inst.type);
    } else if (node->type == MIDPAR_ASTNODETYPE_FUNC_DECL) {
        return midpar_create_func_type(scope, node->func_decl.name);
    } else if (node->type == MIDPAR_ASTNODETYPE_CLASS) {
        return class_node_type(node);
    } else if (node->type == MIDPAR_ASTNODETYPE_TMPLT_PARAM) {
        assert(node->tmplt_param.kind == MIDPAR_TMPLTPARAM_NONTYPE);
        return midpar_copy_type(&node->tmplt_param.non_type.type);
    } else {
        MID_CRASH("fetching the data type of this type of node not supported");
    }
}

static void mark_expr_unknown_ret(struct midpar_Expr *expr)
{
    expr->ret = midpar_create_unknown_type();
}

static void typecheck_strlit_expr(struct midpar_Expr *expr)
{
    // str literals are of type const char[]
    enum midpar_TypeSpec elem_spec;
    switch (expr->type) {
    case MIDPAR_EXPRTYPE_STRING_LIT:
        elem_spec = MIDPAR_TYPESPEC_CHAR;
        break;

    case MIDPAR_EXPRTYPE_WSTRING_LIT:
        elem_spec = MIDPAR_TYPESPEC_WCHAR;
        break;

    case MIDPAR_EXPRTYPE_STRING16_LIT:
        elem_spec = MIDPAR_TYPESPEC_CHAR16;
        break;

    case MIDPAR_EXPRTYPE_STRING32_LIT:
        elem_spec = MIDPAR_TYPESPEC_CHAR32;
        break;

    default:
        MID_CRASH("expr isn't a str lit");
    }

    expr->ret.spec = MIDPAR_TYPESPEC_ARRAY;
    expr->ret.dquals.arr[0].is_const = true;

    expr->ret.array = malloc(sizeof(*expr->ret.array));
    // account for '\0'
    expr->ret.array->len = midlit_strlit_len(&expr->info.val.v.str) + 1;
    expr->ret.array->elem = (struct midpar_Type){.spec = elem_spec};
    midgen_dynpush(&expr->ret.array->elem.dquals,
                   (struct midpar_TypeDataQual){.is_const = true});
}

static void typecheck_lit_expr(struct midpar_Expr *expr)
{
    if (expr->type == MIDPAR_EXPRTYPE_STRING_LIT ||
        expr->type == MIDPAR_EXPRTYPE_WSTRING_LIT ||
        expr->type == MIDPAR_EXPRTYPE_STRING16_LIT ||
        expr->type == MIDPAR_EXPRTYPE_STRING32_LIT)
        expr->valtype = MIDPAR_EXPRVALUE_LVALUE;
    else
        expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;

    midgen_dynpush(&expr->ret.dquals, (struct midpar_TypeDataQual){});

    switch (expr->type) {
    case MIDPAR_EXPRTYPE_CHAR_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_CHAR;
        break;

    case MIDPAR_EXPRTYPE_WCHAR_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_WCHAR;
        break;

    case MIDPAR_EXPRTYPE_CHAR16_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_CHAR16;
        break;

    case MIDPAR_EXPRTYPE_CHAR32_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_CHAR32;
        break;

    case MIDPAR_EXPRTYPE_STRING_LIT:
    case MIDPAR_EXPRTYPE_WSTRING_LIT:
    case MIDPAR_EXPRTYPE_STRING16_LIT:
    case MIDPAR_EXPRTYPE_STRING32_LIT:
        typecheck_strlit_expr(expr);
        break;

    case MIDPAR_EXPRTYPE_INT_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_INT;
        break;
    case MIDPAR_EXPRTYPE_UINT_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_UINT;
        break;

    case MIDPAR_EXPRTYPE_LONG_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_LONG;
        break;
    case MIDPAR_EXPRTYPE_ULONG_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_ULONG;
        break;

    case MIDPAR_EXPRTYPE_LONGLONG_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_LONGLONG;
        break;
    case MIDPAR_EXPRTYPE_ULONGLONG_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_ULONGLONG;
        break;

    case MIDPAR_EXPRTYPE_FLOAT_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_FLOAT;
        break;

    case MIDPAR_EXPRTYPE_DOUBLE_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_DOUBLE;
        break;

    case MIDPAR_EXPRTYPE_LONGDOUBLE_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_LONGDOUBLE;
        break;

    case MIDPAR_EXPRTYPE_BOOL_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_BOOL;
        break;

    case MIDPAR_EXPRTYPE_NULLPTR_LIT:
        expr->ret.spec = MIDPAR_TYPESPEC_NULLPTR;
        break;

    default:
        MID_CRASH("expr isn't a literal");
    }
}

static struct mid_Diag bad_ctor_call_type(const struct midpar_Type *type,
                                          const struct midlex_Token *tok)
{
    char *str = midsema_type_to_str(type);

    struct mid_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("can not call constructor on type '%s'", str),
        .err = MIDDIAG_ERR_BAD_IDENTIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };

    free(str);
    return ret;
}

static void typecheck_ident_expr(struct midpar_Expr *expr,
                                 struct midsema_Scope *scope,
                                 struct mid_DiagVec *diags)
{
    assert(expr->type == MIDPAR_EXPRTYPE_IDENTIFIER);

    auto ident = midsema_find_ident_const(scope, expr->tok->ident);
    if (!ident) {
        midgen_dynpush(diags, middiag_ident_undeclared_err(
                                  expr->tok->ident, expr->tok,
                                  MIDDIAG_ERR_UNDECLARED_IDENTIFIER));
        expr->ret = midpar_create_unknown_type();
        return;
    }
    assert(ident->decl);

    // template non-type parameters are prvalues, all other identifiers are
    // lvalues
    expr->valtype = ident->decl->type == MIDPAR_ASTNODETYPE_TMPLT_PARAM
                        ? MIDPAR_EXPRVALUE_PRVALUE
                        : MIDPAR_EXPRVALUE_LVALUE;

    if (midsema_node_creates_new_type(ident->decl)) {
        // functional cast stuff
        // example: ClassName(1, 2, 3)
        auto type = midsema_type_name_type(scope, expr->tok->ident);

        expr->ret = midpar_toktype_to_type(MIDLEX_TOKENTYPE_INT);
        if (type.spec != MIDPAR_TYPESPEC_CLASS &&
            type.spec != MIDPAR_TYPESPEC_UNION) {
            midgen_dynpush(diags, bad_ctor_call_type(&type, expr->tok));
        } else {
            expr->ret.spec = MIDPAR_TYPESPEC_FUNC;
            expr->ret.func.is_tor = true;
            expr->ret.func.scope = scope;
            expr->ret.func.name = midsema_deref_identptr(&type.named)->name;
        }

        midpar_Type_deinit(&type);
    } else {
        assert(midsema_ident_type(scope, ident, &expr->ret));
    }
}

static struct mid_Diag
this_outside_nonstatic_method_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str(
            "can't use 'this' outside a non-static member function"),
        .err = MIDDIAG_ERR_BAD_THIS_USAGE,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_this_expr(struct midpar_Expr *expr,
                                const struct midsema_Scope *scope,
                                struct mid_DiagVec *diags)
{
    assert(expr->type == MIDPAR_EXPRTYPE_THIS);

    // "this" is a prvalue
    expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;

    auto func_scope =
        midsema_closest_scope_of_type_const(scope, MIDSEMA_SCOPETYPE_FUNC);
    if (!func_scope) {
        midgen_dynpush(diags, this_outside_nonstatic_method_err(expr->tok));
        goto invalid_this;
    }

    const struct midpar_FuncDecl *func = &func_scope->node->func_decl;
    if (!midsema_func_is_method(func) || func->ret.squals.is_static) {
        midgen_dynpush(diags, this_outside_nonstatic_method_err(expr->tok));
        goto invalid_this;
    }

    assert(midpar_func_parent(func)->type == MIDSEMA_SCOPETYPE_CLASS);
    const struct midpar_Class *class_ = &midpar_func_parent(func)->node->class_;

    expr->ret.spec = class_->type == MIDPAR_CLASSTYPE_UNION
                         ? MIDPAR_TYPESPEC_UNION
                         : MIDPAR_TYPESPEC_CLASS;
    expr->ret.named = class_->ident;

    midgen_dynpush(
        &expr->ret.dquals,
        ((struct midpar_TypeDataQual){.is_const = func->quals.is_const,
                                      .is_volatile = func->quals.is_volatile}));
    midgen_dynpush(&expr->ret.dquals, ((struct midpar_TypeDataQual){}));

    return;

invalid_this:
    // default to an int*
    expr->ret = midpar_toktype_to_type(MIDLEX_TOKENTYPE_INT);
    midgen_dynpush(&expr->ret.dquals, ((struct midpar_TypeDataQual){}));
}

// also works with member select operators:
//    var.method();
// becomes:
//    var::method();
static void scope_res_name_impl(const struct midpar_Expr *expr,
                                struct mid_Dynstr *str)
{
    if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER) {
        midstr_append(str, expr->info.ident);
    } else if (expr->type == MIDPAR_EXPRTYPE_THIS) {
        midstr_append(str, "this");
    } else {
        struct midpar_Expr *scope =
            expr->type == MIDPAR_EXPRTYPE_UNARY_SCOPE_RES
                ? NULL
                : &expr->info.args.arr[0];
        struct midpar_Expr *child =
            expr->type == MIDPAR_EXPRTYPE_UNARY_SCOPE_RES
                ? &expr->info.args.arr[0]
                : &expr->info.args.arr[1];

        if (scope && scope->type == MIDPAR_EXPRTYPE_IDENTIFIER)
            midstr_append(str, scope->info.ident);
        else if (scope && scope->type == MIDPAR_EXPRTYPE_THIS)
            midstr_append(str, "this");
        midstr_append(str, "::");

        scope_res_name_impl(child, str);
    }
}

static char *scope_res_name(const struct midpar_Expr *expr)
{
    struct mid_Dynstr ret = {};
    scope_res_name_impl(expr, &ret);
    return ret.str;
}

/*
static const char *scope_res_ident(const struct midpar_Expr *expr)
{
    if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER)
        return expr->info.ident;
    else if (expr->type == MIDPAR_EXPRTYPE_BIN_SCOPE_RES)
        return scope_res_ident(&expr->info.args.arr[1]);
    else if (expr->type == MIDPAR_EXPRTYPE_UNARY_SCOPE_RES)
        return scope_res_ident(&expr->info.args.arr[0]);
    else if (midpar_is_memb_sel(expr->type))
        return scope_res_ident(&expr->info.args.arr[1]);
    else
        MID_CRASH("expr is not a scope resolution");
}
*/

static struct mid_Diag bad_overload_call_err(const char *name,
                                             const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("call to nonexistent overload of '%s'", name),
        .err = MIDDIAG_ERR_BAD_IDENTIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static const struct midpar_TypeDataQual *
method_call_this_quals(struct midpar_Expr *call)
{
    const struct midpar_Expr *lhs = &call->info.args.arr[0];

    const struct midpar_TypeDataQualVec *quals =
        &lhs->info.args.arr[0].ret.dquals;
    return &quals->arr[quals->len - 1];
}

static struct mid_Diag note_func_candidate(const struct midpar_FuncDecl *func)
{
    return (struct mid_Diag){
        .pos = MIDPAR_GET_START(func)->pos,
        .line = MIDPAR_GET_START(func)->line,
        .msg = midcmd_fmt_to_str("candidate not viable"),
        .type = MIDDIAG_TYPE_NOTE,
    };
}

static void note_func_candidates(const char *name,
                                 const struct midpar_Expr *args,
                                 mid_isize n_args, struct midsema_Scope *scope,
                                 bool is_qualified, struct mid_DiagVec *diags)
{
    auto cands =
        midsema_find_candidate_funcs(name, args, n_args, scope, is_qualified);

    for (mid_isize i = 0; i < cands.len; ++i) {
        midgen_dynpush(diags, note_func_candidate(cands.arr[i]));
    }

    midgen_dyndeinit(&cands);
}

static void note_method_candidates(const char *name,

                                   struct midsema_Scope *scope,
                                   struct mid_DiagVec *diags)
{
    auto cands = midsema_find_candidate_methods(name, scope);

    for (mid_isize i = 0; i < cands.len; ++i) {
        midgen_dynpush(diags, note_func_candidate(cands.arr[i]));
    }

    midgen_dyndeinit(&cands);
}

static void set_func_call_node(struct midpar_Expr *expr,
                               struct midsema_Scope *scope,
                               struct mid_DiagVec *diags)
{
    const struct midpar_Expr *lhs = &expr->info.args.arr[0];

    bool qualified =
        midsema_is_scope_res(lhs->type) || midsema_is_memb_sel(lhs->type);
    char *qual_name = scope_res_name(lhs);

    struct midsema_Scope *res =
        lhs->ret.spec == MIDPAR_TYPESPEC_FUNC ? lhs->ret.func.scope : scope;

    if (lhs->ret.spec == MIDPAR_TYPESPEC_FUNC) {
        // bum ass code
        bool is_method = midsema_is_memb_sel(lhs->type);

        if (is_method)
            expr->node = MIDPAR_GET_NODE(midsema_find_method(
                lhs->ret.func.name, &expr->info.args.arr[1],
                expr->info.args.len - 1, res, method_call_this_quals(expr)));
        else
            expr->node = MIDPAR_GET_NODE(
                midsema_find_func(lhs->ret.func.name, &expr->info.args.arr[1],
                                  expr->info.args.len - 1, res, qualified));

        if (!expr->node) {
            midgen_dynpush(diags, bad_overload_call_err(qual_name, lhs->tok));
            if (is_method)
                note_method_candidates(lhs->ret.func.name, res, diags);
            else
                note_func_candidates(
                    lhs->ret.func.name, &expr->info.args.arr[1],
                    expr->info.args.len - 1, res, qualified, diags);
        } else {
#ifdef MIDPAR_DEBUG_LOG_CALLS
            printf("calling func at %d:%d\n", expr->node->start->pos.line,
                   expr->node->start->pos.column);
#endif
        }
    } else if (lhs->ret.spec == MIDPAR_TYPESPEC_FPTR) {
        MID_CRASH("calling function ptrs not implemented");
    } else {
        midgen_dynpush(diags, middiag_func_undeclared_err(
                                  lhs->info.ident, lhs->tok,
                                  MIDDIAG_ERR_UNDECLARED_FUNCTION));
    }

    free(qual_name);
}

static void typecheck_call_expr(struct midpar_Expr *expr,
                                struct midsema_Scope *scope,
                                struct mid_DiagVec *diags)
{
    set_func_call_node(expr, scope, diags);
    if (!expr->node)
        return;

    expr->ret = midpar_copy_type(&expr->node->func_decl.ret);

    if (expr->node->func_decl.ret.lv_ref)
        expr->valtype = MIDPAR_EXPRVALUE_LVALUE;
    else if (expr->node->func_decl.ret.rv_ref)
        expr->valtype = MIDPAR_EXPRVALUE_XVALUE;
    else
        expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;
}

static void typecheck_assignment_expr(struct midpar_Expr *expr,
                                      struct mid_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];

    if (lhs->valtype != MIDPAR_EXPRVALUE_LVALUE)
        midgen_dynpush(
            diags,
            ((struct mid_Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = midcmd_fmt_to_str("can't assign to %s",
                                         lhs->valtype == MIDPAR_EXPRVALUE_XVALUE
                                             ? "an xvalue"
                                             : "a prvalue"),
                .err = MIDDIAG_ERR_BAD_ASSIGNMENT,
                .type = MIDDIAG_TYPE_ERROR,
            }));

    expr->ret = midpar_copy_type(&lhs->ret);
}

static void typecheck_inc_dec_expr(struct midpar_Expr *expr,
                                   struct mid_DiagVec *diags)
{
    bool is_prefix = expr->type == MIDPAR_EXPRTYPE_PREFIX_INC ||
                     expr->type == MIDPAR_EXPRTYPE_PREFIX_DEC;
    bool is_inc = expr->type == MIDPAR_EXPRTYPE_PREFIX_INC ||
                  expr->type == MIDPAR_EXPRTYPE_POSTFIX_INC;

    expr->valtype =
        is_prefix ? MIDPAR_EXPRVALUE_LVALUE : MIDPAR_EXPRVALUE_PRVALUE;

    if (expr->info.args.arr[0].valtype != MIDPAR_EXPRVALUE_LVALUE)
        midgen_dynpush(
            diags,
            ((struct mid_Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = midcmd_fmt_to_str("%s %s requires an lvalue",
                                         is_prefix ? "prefix" : "postfix",
                                         is_inc ? "increment" : "decrement"),
                .err = MIDDIAG_ERR_BAD_ASSIGNMENT,
                .type = MIDDIAG_TYPE_ERROR,
            }));

    expr->ret = midpar_copy_type(&expr->info.args.arr[0].ret);
}

static void typecheck_deref_expr(struct midpar_Expr *expr,
                                 struct mid_DiagVec *diags)
{
    expr->valtype = MIDPAR_EXPRVALUE_LVALUE;

    auto arg = &expr->info.args.arr[0];

    if (midsema_n_indir(&arg->ret) == 0) {
        char *tname = midsema_type_to_str(&arg->ret);
        midgen_dynpush(
            diags,
            ((struct mid_Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = midcmd_fmt_to_str("cannot dereference type '%s'", tname),
                .err = MIDDIAG_ERR_BAD_DEREF,
                .type = MIDDIAG_TYPE_ERROR,
            }));
        free(tname);
        expr->ret = midpar_copy_type(&arg->ret);
    } else {
        bool failed;
        expr->ret = midsema_deref_type(&arg->ret, &failed);
        assert(!failed);
    }
}

static void typecheck_ref_expr(struct midpar_Expr *expr,
                               struct mid_DiagVec *diags)
{
    expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;

    if (expr->info.args.arr[0].valtype != MIDPAR_EXPRVALUE_LVALUE)
        midgen_dynpush(diags,
                       ((struct mid_Diag){
                           .pos = expr->tok->pos,
                           .line = expr->tok->line,
                           .msg = midcmd_fmt_to_str("cannot reference rvalue"),
                           .err = MIDDIAG_ERR_BAD_REF,
                           .type = MIDDIAG_TYPE_ERROR,
                       }));

    bool failed;
    expr->ret = midsema_ref_type(&expr->info.args.arr[0].ret, &failed);
    assert(!failed);
}

static void typecheck_arr_subscr_expr(struct midpar_Expr *expr,
                                      struct mid_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_valid = lhs->ret.spec == MIDPAR_TYPESPEC_ARRAY ||
                     midsema_n_indir(&lhs->ret) > 0;
    bool rhs_valid = rhs->ret.spec == MIDPAR_TYPESPEC_ARRAY ||
                     midsema_n_indir(&rhs->ret) > 0;

    bool lhs_int = midsema_is_integral_typespec(lhs->ret.spec) &&
                   midsema_n_indir(&lhs->ret) == 0;
    bool rhs_int = midsema_is_integral_typespec(rhs->ret.spec) &&
                   midsema_n_indir(&rhs->ret) == 0;

    if (!(lhs_valid && rhs_int) && !(rhs_valid && lhs_int)) {
        char *lhs_tname = midsema_type_to_str(&lhs->ret);
        char *rhs_tname = midsema_type_to_str(&rhs->ret);
        midgen_dynpush(
            diags,
            ((struct mid_Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = midcmd_fmt_to_str("cannot subscript types '%s' and '%s'",
                                         lhs_tname, rhs_tname),
                .err = MIDDIAG_ERR_BAD_ARRAY_SUBSCRIPT,
                .type = MIDDIAG_TYPE_ERROR,
            }));
        free(lhs_tname);
        free(rhs_tname);
        return;
    }

    if ((lhs_valid && lhs->valtype == MIDPAR_EXPRVALUE_LVALUE) ||
        (rhs_valid && rhs->valtype == MIDPAR_EXPRVALUE_LVALUE) ||
        midsema_n_indir(&lhs->ret) > 0 || midsema_n_indir(&rhs->ret) > 0) {
        expr->valtype = MIDPAR_EXPRVALUE_LVALUE;
    } else {
        expr->valtype = MIDPAR_EXPRVALUE_XVALUE;
    }

    const struct midpar_Expr *arr = lhs_valid ? lhs : rhs;

    if (midsema_n_indir(&arr->ret) > 0) {
        expr->ret = midsema_deref_type(&arr->ret, nullptr);
    } else {
        assert(midsema_type_is_array(&arr->ret));
        expr->ret = midpar_copy_type(&arr->ret.array->elem);
    }
}

static void typecheck_comma_expr(struct midpar_Expr *expr)
{
    auto rhs = &expr->info.args.arr[1];

    expr->valtype = rhs->valtype;
    expr->ret = midpar_copy_type(&rhs->ret);
}

static struct mid_Diag cond_one_result_void_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = strdup("only one result of conditional '?' expression is void"),
        .err = MIDDIAG_ERR_BAD_CONDITIONAL,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_conditional_expr(struct midpar_Expr *expr,
                                       struct mid_DiagVec *diags)
{
    // e1 ? e2 : e3

    (void)cond_one_result_void_err(expr->tok);
    (void)expr;
    (void)diags;
    MID_CRASH("haven't implemented typechecking the conditional operator");

#if 0
    auto e2 = &expr->info.args.arr[1];
    auto e3 = &expr->info.args.arr[2];

    bool e2_void =
        e2->ret.spec == MIDPAR_TYPESPEC_VOID && midsema_n_indir(&e2->ret) == 0;
    bool e3_void =
        e3->ret.spec == MIDPAR_TYPESPEC_VOID && midsema_n_indir(&e3->ret) == 0;

    if (e2_void && e3_void) {
        expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;
        expr->ret = midpar_copy_type(&e2->ret);
    } else if (e2_void) {
        if (e3->type != MIDPAR_EXPRTYPE_THROW)
            midgen_dynpush(diags, cond_one_result_void_err(e3->tok));
        expr->valtype = e3->valtype;
        expr->ret = midpar_copy_type(&e3->ret);
    } else if (e3_void) {
        if (e2->type != MIDPAR_EXPRTYPE_THROW)
            midgen_dynpush(diags, cond_one_result_void_err(e2->tok));
        expr->valtype = e2->valtype;
        expr->ret = midpar_copy_type(&e2->ret);
    }
#endif
}

static struct mid_Diag bad_operands(const struct midpar_Expr *expr,
                                    const char *type,
                                    enum middiag_ErrT err_type)
{
    bool unary = expr->info.args.len == 1;

    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    char *lhs_tname = midsema_type_to_str(&lhs->ret);
    char *rhs_tname = unary ? NULL : midsema_type_to_str(&rhs->ret);
    struct mid_Diag ret;
    if (unary) {
        ret = (struct mid_Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg = midcmd_fmt_to_str("%s operator can not operate on '%s'",
                                     type, lhs_tname),
            .err = err_type,
            .type = MIDDIAG_TYPE_ERROR,
        };
    } else {
        ret = (struct mid_Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg = midcmd_fmt_to_str(
                "%s operator can not operate on '%s' and '%s'", type, lhs_tname,
                rhs_tname),
            .err = err_type,
            .type = MIDDIAG_TYPE_ERROR,
        };
    }
    free(lhs_tname);
    free(rhs_tname);

    return ret;
}

static enum midpar_TypeSpec op_prom_typespec(enum midpar_TypeSpec spec)
{
    if (midsema_is_integral_typespec(spec))
        return midsema_integral_prom(spec);
    else if (midsema_is_floating_typespec(spec))
        return spec;
    else {
        printf("type spec = %d\n", spec);
        /*
         *(volatile int *)NULL = 10;
         */
        MID_CRASH("can't promote type spec");
    }
}

static struct midpar_Type op_prom_type(struct midpar_Type *type)
{
    auto ret = midpar_copy_type(type);
    ret.spec = op_prom_typespec(ret.spec);
    return ret;
}

static bool is_ptr_arith_op(enum midpar_ExprType op)
{
    return op == MIDPAR_EXPRTYPE_ADD || op == MIDPAR_EXPRTYPE_SUB;
}

static bool is_flt_arith_op(enum midpar_ExprType op)
{
    return op == MIDPAR_EXPRTYPE_ADD || op == MIDPAR_EXPRTYPE_SUB ||
           op == MIDPAR_EXPRTYPE_MUL || op == MIDPAR_EXPRTYPE_DIV ||
           op == MIDPAR_EXPRTYPE_UNARY_PLUS ||
           op == MIDPAR_EXPRTYPE_UNARY_MINUS;
}

static void typecheck_arith_bin_op_expr(struct midpar_Expr *expr,
                                        struct mid_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_ptr = midsema_n_indir(&lhs->ret) > 0;
    bool rhs_ptr = midsema_n_indir(&rhs->ret) > 0;

    bool bad_op_types;
    if (is_ptr_arith_op(expr->type)) {
        if (lhs_ptr && rhs_ptr)
            bad_op_types = true;
        else if (lhs_ptr)
            bad_op_types = !midsema_is_integral_typespec(rhs->ret.spec);
        else if (rhs_ptr)
            bad_op_types = midsema_is_integral_typespec(lhs->ret.spec);
        else
            bad_op_types = false;
    } else {
        bad_op_types = lhs_ptr || rhs_ptr;
    }

    bool lhs_flt = !lhs_ptr && midsema_is_floating_typespec(lhs->ret.spec);
    bool rhs_flt = !rhs_ptr && midsema_is_floating_typespec(rhs->ret.spec);

    if (bad_op_types) {
        midgen_dynpush(diags, bad_operands(expr, "arithmetic",
                                           MIDDIAG_ERR_BAD_ARITHMETIC_OP));
        expr->ret = midpar_create_unknown_type();
    } else if (lhs_ptr) {
        expr->ret = midpar_copy_type(&lhs->ret);
    } else if (rhs_ptr) {
        expr->ret = midpar_copy_type(&rhs->ret);
    } else if ((lhs_flt || rhs_flt) && !is_flt_arith_op(expr->type)) {
        const char *name =
            expr->type == MIDPAR_EXPRTYPE_MOD ? "module" : "bitwise";
        midgen_dynpush(diags,
                       bad_operands(expr, name, MIDDIAG_ERR_BAD_ARITHMETIC_OP));
        expr->ret = midpar_create_unknown_type();
    } else {
        int32_t lhs_rank =
            midsema_typespec_conv_rank(op_prom_typespec(lhs->ret.spec));
        int32_t rhs_rank =
            midsema_typespec_conv_rank(op_prom_typespec(rhs->ret.spec));
        expr->ret = lhs_rank > rhs_rank ? op_prom_type(&lhs->ret)
                                        : op_prom_type(&rhs->ret);
    }
}

static void typecheck_arith_unary_op_expr(struct midpar_Expr *expr,
                                          struct mid_DiagVec *diags)
{
    auto arg = &expr->info.args.arr[0];
    expr->ret = op_prom_type(&arg->ret);

    bool arg_ptr = midsema_n_indir(&arg->ret) > 0;

    bool bad_op_types = arg_ptr;

    if (bad_op_types) {
        midgen_dynpush(diags, bad_operands(expr, "arithmetic",
                                           MIDDIAG_ERR_BAD_ARITHMETIC_OP));
    }
}

static void typecheck_arith_op_expr(struct midpar_Expr *expr,
                                    struct mid_DiagVec *diags)
{
    expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;

    if (midsema_is_unaryop(expr->type))
        typecheck_arith_unary_op_expr(expr, diags);
    else
        typecheck_arith_bin_op_expr(expr, diags);
}

// TODO: implement these
static void typecheck_logical_unary_op_expr(struct midpar_Expr *expr,
                                            struct mid_DiagVec *diags)
{
    (void)expr;
    (void)diags;
}

static void typecheck_logical_bin_op_expr(struct midpar_Expr *expr,
                                          struct mid_DiagVec *diags)
{
    (void)expr;
    (void)diags;
}

static void typecheck_logical_op_expr(struct midpar_Expr *expr,
                                      struct mid_DiagVec *diags)
{
    MID_CRASH("typechecking logical op exprs hasn't been implemented yet");

    expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;
    expr->ret = midpar_toktype_to_type(MIDLEX_TOKENTYPE_BOOL);

    if (midsema_is_unaryop(expr->type))
        typecheck_logical_unary_op_expr(expr, diags);
    else
        typecheck_logical_bin_op_expr(expr, diags);
}

static void typecheck_comp_op_expr(struct midpar_Expr *expr,
                                   struct mid_DiagVec *diags)
{
    expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;
    expr->ret = midpar_toktype_to_type(MIDLEX_TOKENTYPE_BOOL);

    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_ptr = midsema_n_indir(&lhs->ret) > 0;
    bool lhs_void_ptr =
        lhs->ret.spec == MIDPAR_TYPESPEC_VOID && midsema_n_indir(&lhs->ret);
    bool rhs_ptr = midsema_n_indir(&rhs->ret) > 0;
    bool rhs_void_ptr =
        rhs->ret.spec == MIDPAR_TYPESPEC_VOID && midsema_n_indir(&rhs->ret);

    bool eq = lhs->ret.spec == rhs->ret.spec && midsema_n_indir(&lhs->ret) &&
              midsema_n_indir(&rhs->ret);

    // an arithmetic operator can operate on primitives, ptrs of the same type,
    // or a ptr and a void ptr
    bool bad_op_types =
        (lhs_ptr || rhs_ptr) && (!eq && (!lhs_void_ptr && !rhs_void_ptr));

    if (bad_op_types) {
        midgen_dynpush(
            diags, bad_operands(expr, "comp", MIDDIAG_ERR_BAD_COMPARISON_OP));
    }
}

static struct midsema_Scope *bin_scope_res_scope(struct midpar_Expr *expr,
                                                 struct midsema_Scope *scope)
{
    auto lhs = &expr->info.args.arr[0];
    assert(lhs->type == MIDPAR_EXPRTYPE_IDENTIFIER);

    struct midsema_Scope *base = midsema_closest_rnce_scope(scope);

    const char *name = lhs->info.ident;
    struct midsema_Scope *res = midsema_resolve_scope(name, base);

    assert(res);
    return res;
}

static struct midsema_Scope *unary_scope_res_scope(struct midsema_Scope *scope)
{
    auto res = scope;
    while (res->parent)
        res = res->parent;
    return res;
}

static void typecheck_scope_res_expr(struct midpar_Expr *expr,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags)
{
    struct midsema_Scope *res;
    struct midpar_Expr *arg;
    if (expr->type == MIDPAR_EXPRTYPE_BIN_SCOPE_RES) {
        res = bin_scope_res_scope(expr, scope);
        arg = &expr->info.args.arr[1];
    } else {
        res = unary_scope_res_scope(scope);
        arg = &expr->info.args.arr[0];
    }

    typecheck_expr(arg, res, diags, false);

    expr->ret = midpar_copy_type(&arg->ret);
    expr->valtype = arg->valtype;
    expr->res_scope = midsema_is_scope_res(arg->type) ? arg->res_scope : res;
}

static struct mid_Diag
memb_sel_lhs_not_class_err(const struct midpar_Expr *memb_sel)
{
    char *lhs_type = midsema_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct mid_Diag ret = {
        .pos = memb_sel->tok->pos,
        .line = memb_sel->tok->line,
        .msg = midcmd_fmt_to_str(
            "member select lhs '%s' is not a class or union", lhs_type),
        .err = MIDDIAG_ERR_BAD_MEMB_SEL,
        .type = MIDDIAG_TYPE_ERROR};

    free(lhs_type);
    return ret;
}

static struct mid_Diag
memb_sel_expects_ptr_err(const struct midpar_Expr *memb_sel)
{
    char *lhs_type = midsema_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct mid_Diag ret = {
        .pos = memb_sel->tok->pos,
        .line = memb_sel->tok->line,
        .msg = midcmd_fmt_to_str("member select lhs '%s' is not a pointer",
                                 lhs_type),
        .err = MIDDIAG_ERR_BAD_MEMB_SEL,
        .type = MIDDIAG_TYPE_ERROR};

    free(lhs_type);
    return ret;
}

static struct mid_Diag
memb_sel_expects_non_ptr_err(const struct midpar_Expr *memb_sel)
{
    char *lhs_type = midsema_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct mid_Diag ret = {.pos = memb_sel->tok->pos,
                           .line = memb_sel->tok->line,
                           .msg = midcmd_fmt_to_str(
                               "member select lhs '%s' is a pointer", lhs_type),
                           .err = MIDDIAG_ERR_BAD_MEMB_SEL,
                           .type = MIDDIAG_TYPE_ERROR};

    free(lhs_type);
    return ret;
}

static bool memb_sel_expects_ptr(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_PTR_MEMB_SEL ||
           type == MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
}

static struct mid_Diag unknown_field_err(const char *field, const char *class_,
                                         bool is_union,
                                         const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("unknown field '%s' in %s '%s'", field,
                                 is_union ? "union" : "class", class_),
        .err = MIDDIAG_ERR_BAD_IDENTIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_memb_sel(struct midpar_Expr *expr,
                               struct midsema_Scope *scope,
                               struct mid_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    typecheck_expr(lhs, scope, diags, false);
    if (!midsema_type_is_typecheckable(&lhs->ret)) {
        mark_expr_unknown_ret(expr);
        return;
    }

    if (lhs->ret.spec != MIDPAR_TYPESPEC_CLASS &&
        lhs->ret.spec != MIDPAR_TYPESPEC_UNION) {
        midgen_dynpush(diags, memb_sel_lhs_not_class_err(expr));
        return;
    } else if (memb_sel_expects_ptr(expr->type) &&
               midsema_n_indir(&lhs->ret) != 1) {
        midgen_dynpush(diags, memb_sel_expects_ptr_err(expr));
    } else if (!memb_sel_expects_ptr(expr->type) &&
               midsema_n_indir(&lhs->ret) != 0) {
        midgen_dynpush(diags, memb_sel_expects_non_ptr_err(expr));
    } else if (rhs->type != MIDPAR_EXPRTYPE_IDENTIFIER) {
        midgen_dynpush(
            diags, middiag_expected_token_err("identifier", expr->tok,
                                              MIDDIAG_ERR_MISSING_IDENTIFIER));
        return;
    }

    const struct midpar_Class *class_ =
        &midsema_deref_identptr(&lhs->ret.named)->decl->class_;
    const char *field_name = rhs->info.ident;
    const struct midpar_ASTNode *field = midsema_find_field(class_, field_name);
    if (!field) {
        midgen_dynpush(diags,
                       unknown_field_err(field_name, class_->name,
                                         class_->type == MIDPAR_CLASSTYPE_UNION,
                                         rhs->tok));
        return;
    }

    if (field->type == MIDPAR_ASTNODETYPE_VAR_DECL_INST) {
        expr->ret = midpar_copy_type(&field->var_inst.type);
        expr->valtype = MIDPAR_EXPRVALUE_LVALUE;
    } else {
        assert(field->type == MIDPAR_ASTNODETYPE_FUNC_DECL);
        expr->ret = midpar_create_func_type(
            midsema_deref_identptr(&class_->ident)->class_info.def_scope,
            field->func_decl.name);
        expr->valtype = MIDPAR_EXPRVALUE_LVALUE;
    }
}

static void typecheck_sizeof_expr(struct midpar_Expr *expr)
{
    expr->ret = midpar_create_simple_type(MIDPAR_TYPEALIAS_SIZET, 0);
    expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;
}

static void typecheck_op_expr(struct midpar_Expr *expr,
                              struct midsema_Scope *scope,
                              struct mid_DiagVec *diags)
{
    if (expr->type == MIDPAR_EXPRTYPE_FUNC_CALL)
        typecheck_call_expr(expr, scope, diags);
    else if (midsema_is_assignment(expr->type))
        typecheck_assignment_expr(expr, diags);
    else if (expr->type == MIDPAR_EXPRTYPE_PREFIX_INC ||
             expr->type == MIDPAR_EXPRTYPE_PREFIX_DEC ||
             expr->type == MIDPAR_EXPRTYPE_POSTFIX_INC ||
             expr->type == MIDPAR_EXPRTYPE_POSTFIX_DEC)
        typecheck_inc_dec_expr(expr, diags);
    else if (expr->type == MIDPAR_EXPRTYPE_DEREF)
        typecheck_deref_expr(expr, diags);
    else if (expr->type == MIDPAR_EXPRTYPE_REF)
        typecheck_ref_expr(expr, diags);
    else if (expr->type == MIDPAR_EXPRTYPE_ARRAY_SUBSCR)
        typecheck_arr_subscr_expr(expr, diags);
    else if (expr->type == MIDPAR_EXPRTYPE_COMMA)
        typecheck_comma_expr(expr);
    else if (expr->type == MIDPAR_EXPRTYPE_CONDITIONAL)
        typecheck_conditional_expr(expr, diags);
    else if (midsema_is_arith_op(expr->type))
        typecheck_arith_op_expr(expr, diags);
    else if (midsema_is_logical_op(expr->type))
        typecheck_logical_op_expr(expr, diags);
    else if (midsema_is_comp_op(expr->type))
        typecheck_comp_op_expr(expr, diags);
    else if (midsema_is_scope_res(expr->type))
        typecheck_scope_res_expr(expr, scope, diags);
    else if (midsema_is_memb_sel(expr->type))
        typecheck_memb_sel(expr, scope, diags);
    else if (expr->type == MIDPAR_EXPRTYPE_SIZEOF)
        typecheck_sizeof_expr(expr);
    else {
        printf("op at %d:%d\n", expr->tok->pos.line, expr->tok->pos.column);
        printf("op type = %d\n", expr->type);
        MID_CRASH("typechecking op not implemented");
    }
}

static void typecheck_overloaded_op(struct midpar_Expr *expr,
                                    struct midpar_FuncDecl *overload)
{
#ifdef MIDPAR_DEBUG_LOG_CALLS
    printf("found op overload at %d:%d\n", expr->tok->pos.line,
           expr->tok->pos.column);
    printf("op overload decl at %d:%d\n", MIDPAR_GET_START(overload)->pos.line,
           MIDPAR_GET_START(overload)->pos.column);
#endif

    expr->overloaded = true;
    expr->node = MIDPAR_GET_NODE(overload);
    expr->ret = midpar_copy_type(&overload->ret);

    if (overload->ret.lv_ref)
        expr->valtype = MIDPAR_EXPRVALUE_LVALUE;
    else if (overload->ret.rv_ref)
        expr->valtype = MIDPAR_EXPRVALUE_XVALUE;
    else
        expr->valtype = MIDPAR_EXPRVALUE_PRVALUE;
}

static bool has_no_untypecheckable_args(struct midpar_Expr *expr)
{
    for (mid_isize i = 0; i < expr->info.args.len; ++i) {
        if (!midsema_type_is_typecheckable(&expr->info.args.arr[i].ret))
            return false;
    }

    return true;
}

// root - is expr the root of the expression tree
static void typecheck_expr(struct midpar_Expr *expr,
                           struct midsema_Scope *scope,
                           struct mid_DiagVec *diags, bool root)
{
    if (expr->typechecked)
        return;

    if (midsema_is_numlit(expr->type)) {
        typecheck_lit_expr(expr);
    } else if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER) {
        typecheck_ident_expr(expr, scope, diags);
    } else if (expr->type == MIDPAR_EXPRTYPE_THIS) {
        typecheck_this_expr(expr, scope, diags);
    } else {
        // some operators are weird
        bool typecheck_args = !midsema_is_scope_res(expr->type) &&
                              !midsema_is_memb_sel(expr->type);
        if (typecheck_args) {
            for (mid_isize i = 0; i < expr->info.args.len; ++i)
                typecheck_expr(&expr->info.args.arr[i], scope, diags, false);

            if (!has_no_untypecheckable_args(expr)) {
                mark_expr_unknown_ret(expr);
                expr->typechecked = true;
                return;
            }
        }

        struct midpar_FuncDecl *overload = midsema_find_op_overload(
            expr->type, expr->info.args.arr, expr->info.args.len, scope);
        if (!overload)
            typecheck_op_expr(expr, scope, diags);
        else
            typecheck_overloaded_op(expr, overload);
    }

    expr->typechecked = true;

    if (root)
        midsema_const_fold_expr(expr, scope, false);
}

void midsema_typecheck_expr(struct midpar_Expr *expr,
                            struct midsema_Scope *scope,
                            struct mid_DiagVec *diags)
{
    typecheck_expr(expr, scope, diags, true);
}

static struct mid_Diag no_matching_ctor_err(const struct midpar_Type *type,
                                            const struct midlex_Token *tok)
{
    char *str = midsema_type_to_str(type);

    struct mid_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("no matching constructor for '%s'", str),
        .err = MIDDIAG_ERR_NO_MATCHING_CTOR,
        .type = MIDDIAG_TYPE_ERROR,
    };

    free(str);
    return ret;
}

// returns whether or not the ctor is correct
static struct midpar_FuncDecl *
typecheck_class_type_ctor_call(const struct midpar_Type *type,
                               const struct midpar_Expr *args, mid_isize n_args)
{
    auto ident = midsema_deref_identptr(&type->named);
    return midsema_find_func(ident->name, args, n_args,
                             ident->class_info.def_scope, true);
}

// returns whether or not the ctor is correct
static bool typecheck_generic_type_ctor_call(const struct midpar_Type *type,
                                             const struct midpar_Expr *args,
                                             mid_isize n_args)
{
    if (n_args > 1)
        return false;
    else if (n_args == 0)
        return midsema_type_is_default_constructible(type);

    return midsema_can_convert(&args[0].ret, args[0].valtype, type);
}

// returns true if a ctor was found, false otherwise.
// out_ctor       - can not be NULL. set to NULL if there isn't a ctor, tho
//                  that doesn't necessarily mean the function failed.
bool typecheck_ctor_call(const struct midpar_Type *type,
                         const struct midpar_Expr *args, mid_isize n_args,
                         struct midpar_FuncDecl **out_ctor)
{
    if (midsema_type_is_class_or_union(type)) {
        *out_ctor = typecheck_class_type_ctor_call(type, args, n_args);
        return *out_ctor != nullptr;
    } else {
        *out_ctor = nullptr;
        return typecheck_generic_type_ctor_call(type, args, n_args);
    }
}

static void typecheck_vdecl_ctor_call(struct midpar_VarDeclInst *inst,
                                      struct mid_DiagVec *diags)
{
    bool failed = typecheck_ctor_call(&inst->type, inst->ctor.args.arr,
                                      inst->ctor.args.len, &inst->ctor.ctor);

    if (failed)
        midgen_dynpush(
            diags, no_matching_ctor_err(&inst->type, MIDPAR_GET_START(inst)));
}

static struct mid_Diag
uninited_constexpr_var_err(const char *name, const struct midlex_Token *tok)
{
    struct mid_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("constexpr variable '%s' must be initialized",
                                 name),
        .err = MIDDIAG_ERR_NO_MATCHING_CTOR,
        .type = MIDDIAG_TYPE_ERROR,
    };

    return ret;
}

static struct mid_Diag
constexpr_var_init_not_constexpr_err(const char *name,
                                     const struct midlex_Token *tok)
{
    struct mid_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str(
            "constexpr variable '%s' must have a constexpr initializer", name),
        .err = MIDDIAG_ERR_NO_MATCHING_CTOR,
        .type = MIDDIAG_TYPE_ERROR,
    };

    return ret;
}

static struct mid_Diag
constexpr_var_not_literal_type_err(const char *name,
                                   const struct midpar_Type *type,
                                   const struct midlex_Token *tok)
{
    char *type_str = midsema_type_to_str(type);

    struct mid_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("constexpr variable '%s' is of type '%s' "
                                 "which is not a literal type",
                                 name, type_str),
        .err = MIDDIAG_ERR_NO_MATCHING_CTOR,
        .type = MIDDIAG_TYPE_ERROR,
    };

    free(type_str);
    return ret;
}

static void typecheck_constexpr_var(struct midpar_VarDeclInst *inst,
                                    struct midsema_Scope *scope,
                                    struct mid_DiagVec *diags)
{
    if (!midsema_type_is_literal(&inst->type))
        midgen_dynpush(
            diags, constexpr_var_not_literal_type_err(inst->name, &inst->type,
                                                      MIDPAR_GET_START(inst)));

    if (!inst->has_ctor) {
        if (inst->init.expr && !inst->init.expr->constant) {
            midgen_dynpush(diags, constexpr_var_init_not_constexpr_err(
                                      inst->name, MIDPAR_GET_START(inst)));
        } else if (inst->init.expr) {
            inst->constexpr_val = mid_malloc(sizeof(*inst->constexpr_val));
            bool failed;
            *inst->constexpr_val =
                midsema_eval_expr(inst->init.expr, scope, &failed);
            assert(!failed);
            // we gotta apply any potential implicit type conversions to make
            // sure the value is the right type
            midlit_convert_value(inst->constexpr_val, &inst->type);
        } else if (midsema_type_is_constexpr_default_constructible(
                       &inst->type)) {
            inst->constexpr_val = mid_malloc(sizeof(*inst->constexpr_val));
            assert(midsema_constexpr_default_init_type(&inst->type,
                                                       inst->constexpr_val));
        } else {
            midgen_dynpush(diags, uninited_constexpr_var_err(
                                      inst->name, MIDPAR_GET_START(inst)));
        }
    } else {
        MID_CRASH("calling constexpr ctors not supported yet");
    }
}

void midsema_typecheck_var_decl_inst(struct midpar_VarDeclInst *inst,
                                     struct midsema_Scope *scope,
                                     struct mid_DiagVec *diags)
{
    inst->typechecked = true;

    if (inst->has_ctor)
        typecheck_vdecl_ctor_call(inst, diags);
    else if (inst->init.expr)
        midsema_typecheck_expr(inst->init.expr, scope, diags);

    if (inst->type.squals.is_constexpr)
        typecheck_constexpr_var(inst, scope, diags);
}

static struct mid_Diag
ctor_memb_init_not_in_ctor_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("constructor member initializer lists can "
                                 "only appear in constructors"),
        .err = MIDDIAG_ERR_BAD_CTOR_MEMB_INIT_LIST,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct mid_Diag
ctor_field_isnt_nonstatic_dmemb_err(const char *name,
                                    const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("field '%s' is not a non-static data member",
                                 name),
        .err = MIDDIAG_ERR_BAD_CTOR_MEMB_INIT_LIST,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_ctor_memb_init(const struct midpar_CtorMemberInit *init,
                                     const struct midpar_Class *class,
                                     struct mid_DiagVec *diags)
{
    if (!midsema_field_is_nonstatic_data_memb(class, init->name)) {
        midgen_dynpush(diags, ctor_field_isnt_nonstatic_dmemb_err(
                                  init->name, MIDPAR_GET_START(init)));
        return;
    }

    const struct midpar_VarDeclInst *field =
        &midsema_find_field(class, init->name)->var_inst;

    struct midpar_FuncDecl *ctor;
    if (!typecheck_ctor_call(&field->type, init->args, init->n_args, &ctor))
        midgen_dynpush(
            diags, no_matching_ctor_err(&field->type, MIDPAR_GET_START(init)));
}

static void typecheck_ctor_memb_init_list(const struct midpar_FuncDecl *func,
                                          struct mid_DiagVec *diags)
{
    if (!midsema_func_is_ctor(func)) {
        midgen_dynpush(diags, ctor_memb_init_not_in_ctor_err(
                                  MIDPAR_GET_START(func->memb_inits.arr[0])));
        return;
    }

    const struct midpar_ASTNode *class = MIDPAR_GET_PARENT(func);
    assert(class->type == MIDPAR_ASTNODETYPE_CLASS);

    for (mid_isize i = 0; i < func->memb_inits.len; ++i) {
        typecheck_ctor_memb_init(func->memb_inits.arr[i], &class->class_,
                                 diags);
    }
}

static struct mid_Diag constexpr_unsuitable_err(const char *name,
                                                const char *reason,
                                                const struct midlex_Token *tok)
{
    const char *fmt = reason
                          ? "function '%s' is not constexpr suitable because %s"
                          : "function '%s' is not constexpr suitable";

    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str(fmt, name, reason),
        .err = MIDDIAG_ERR_BAD_CONSTEXPR,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

// check_body       - if false, only the declaration is checked for constexpr
//                    suitability, otherwise only the body is checked
static void verify_constexpr_suitability(const struct midpar_FuncDecl *self,
                                         bool check_body,
                                         struct mid_DiagVec *diags)
{
    auto suitability = check_body
                           ? midsema_func_body_constexpr_suitability(self)
                           : midsema_func_decl_constexpr_suitability(self);

    switch (suitability) {
    case MIDSEMA_FUNCCONSTEXPR_SUITABLE:
        break;

    case MIDSEMA_FUNCCONSTEXPR_NONLITERAL_RET:
        midgen_dynpush(diags, constexpr_unsuitable_err(
                                  self->name, "its return type is non-literal",
                                  MIDPAR_GET_START(self)));
        break;

    case MIDSEMA_FUNCCONSTEXPR_NONLITERAL_PARAM:
        midgen_dynpush(diags,
                       constexpr_unsuitable_err(
                           self->name, "it has a non-literal parameter type",
                           MIDPAR_GET_START(self)));
        break;

    case MIDSEMA_FUNCCONSTEXPR_VIRTUAL:
        midgen_dynpush(diags,
                       constexpr_unsuitable_err(self->name, "it's virtual",
                                                MIDPAR_GET_START(self)));
        break;

    case MIDSEMA_FUNCCONSTEXPR_NONLITERAL_CTOR:
        midgen_dynpush(diags, constexpr_unsuitable_err(
                                  self->name,
                                  "it's a constructor of a non-literal class",
                                  MIDPAR_GET_START(self)));
        break;

    case MIDSEMA_FUNCCONSTEXPR_RET_IN_CTOR:
        midgen_dynpush(diags, constexpr_unsuitable_err(
                                  self->name, "it has a return statement",
                                  MIDPAR_GET_START(self)));
        break;

    case MIDSEMA_FUNCCONSTEXPR_MULTIPLE_RET:
        midgen_dynpush(
            diags, constexpr_unsuitable_err(self->name,
                                            "it has multiple return statements",
                                            MIDPAR_GET_START(self)));
        break;

    case MIDSEMA_FUNCCONSTEXPR_BAD_BODY:
        midgen_dynpush(diags, constexpr_unsuitable_err(self->name, nullptr,
                                                       MIDPAR_GET_START(self)));
        break;
    }
}

void midsema_typecheck_func_decl(struct midpar_FuncDecl *func,
                                 struct mid_DiagVec *diags)
{
    if (func->quals.is_constexpr)
        verify_constexpr_suitability(func, false, diags);
}

void midsema_typecheck_func_body(struct midpar_FuncDecl *func,
                                 struct mid_DiagVec *diags)
{
    if (func->memb_inits.len > 0)
        typecheck_ctor_memb_init_list(func, diags);

    if (func->quals.is_constexpr)
        verify_constexpr_suitability(func, true, diags);
}

static struct mid_Diag
invalid_return_stmt_type_err(const struct midpar_Type *func_type,
                             const struct midpar_Type *ret_type,
                             const struct midlex_Token *tok)
{
    char *func_type_str = midsema_type_to_str(func_type);

    struct mid_Diag ret;

    if (ret_type) {
        char *ret_type_str = midsema_type_to_str(ret_type);

        ret = (struct mid_Diag){
            .pos = tok->pos,
            .line = tok->line,
            .msg = midcmd_fmt_to_str("returning '%s' in function of type '%s'",
                                     ret_type_str, func_type_str),
            .err = MIDDIAG_ERR_BAD_RETURN_STMT_TYPE,
            .type = MIDDIAG_TYPE_ERROR,
        };

        free(ret_type_str);
    } else {
        ret = (struct mid_Diag){
            .pos = tok->pos,
            .line = tok->line,
            .msg = midcmd_fmt_to_str(
                "expected a return value in function of type '%s'",
                func_type_str),
            .err = MIDDIAG_ERR_BAD_RETURN_STMT_TYPE,
            .type = MIDDIAG_TYPE_ERROR,
        };
    }

    free(func_type_str);

    return ret;
}

static struct mid_Diag return_outside_func_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("return statement outside a function"),
        .err = MIDDIAG_ERR_RETURN_OUTSIDE_FUNC,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

void midsema_typecheck_return(struct midpar_Return *self,
                              const struct midsema_Scope *scope,
                              struct mid_DiagVec *diags)
{
    auto func_scope =
        midsema_closest_scope_of_type_const(scope, MIDSEMA_SCOPETYPE_FUNC);
    if (!func_scope) {
        midgen_dynpush(diags, return_outside_func_err(MIDPAR_GET_START(self)));
        return;
    }

    const struct midpar_Type *func_type = &func_scope->node->func_decl.ret;
    if (!midsema_type_is_typecheckable(func_type))
        return;

    bool is_void = func_type->spec == MIDPAR_TYPESPEC_VOID &&
                   midsema_n_indir(func_type) == 0;

    if (self->expr) {
        if (!midsema_type_is_typecheckable(&self->expr->ret))
            return;
        if (!midsema_can_convert(&self->expr->ret, self->expr->valtype,
                                 func_type))
            midgen_dynpush(
                diags, invalid_return_stmt_type_err(func_type, &self->expr->ret,
                                                    MIDPAR_GET_START(self)));
    } else if (!is_void) {
        midgen_dynpush(diags, invalid_return_stmt_type_err(
                                  func_type, NULL, MIDPAR_GET_START(self)));
    }
}

static bool is_valid_array_to_ptr(const struct midpar_Type *src,
                                  const struct midpar_Type *dest)
{
    if (src->spec != MIDPAR_TYPESPEC_ARRAY)
        return false;

    // an array to ptr conversion becomes a prvalue so it can't be passed to a
    // non-const lvalue reference
    if (dest->lv_ref && !dest->dquals.arr[0].is_const)
        return false;

    mid_isize src_indir = midsema_n_indir(src);
    mid_isize elem_indir = midsema_n_indir(&src->array->elem);
    mid_isize dest_indir = midsema_n_indir(dest);

    if (src_indir != 0 || elem_indir + 1 != dest_indir)
        return false;
    else if (src->array->elem.spec != dest->spec)
        return false;
    else if (!midsema_dquals_same(src->array->elem.dquals.arr,
                                  src->array->elem.dquals.len,
                                  &dest->dquals.arr[1], dest->dquals.len - 1))
        return false;

    return true;
}

bool midsema_can_convert(const struct midpar_Type *src,
                         enum midpar_ExprValueType src_valtype,
                         const struct midpar_Type *dest)
{
    // rv references cannot take lvalues and non-const lv rereferences
    // cannot take rvalues
    if ((dest->rv_ref && !midsema_is_rvalue(src_valtype)) ||
        (dest->lv_ref && !dest->dquals.arr[0].is_const &&
         midsema_is_rvalue(src_valtype)))
        return false;

    bool src_ptr = midsema_n_indir(src) > 0;
    bool dest_ptr = midsema_n_indir(dest) > 0;

    if (!src_ptr && !dest_ptr && midsema_type_is_scalar(src) &&
        midsema_type_is_scalar(dest))
        return true;
    else if (midsema_n_indir(src) == midsema_n_indir(dest) &&
             src->spec == dest->spec)
        return true;
    else if (src_ptr && midsema_type_is_void_ptr(dest))
        return true;
    else if (midsema_type_is_nullptr_t(src) && dest_ptr)
        return true;
    else if (is_valid_array_to_ptr(src, dest))
        return true;

    return false;
}

int midsema_conversion_rank(const struct midpar_Type *src,
                            const struct midpar_Type *dest)
{
    bool same_indir = midsema_n_indir(src) == midsema_n_indir(dest);
    bool no_indir = midsema_n_indir(src) == 0 && midsema_n_indir(dest) == 0;

    bool src_int = midsema_is_integral_typespec(src->spec);
    bool dest_int = midsema_is_integral_typespec(dest->spec);
    bool src_flt = midsema_is_floating_typespec(src->spec);
    bool dest_flt = midsema_is_floating_typespec(dest->spec);

    if (same_indir && src->spec == dest->spec)
        return 1;
    else if (no_indir && ((src_int && dest_int) || (src_flt && dest_flt)))
        return 2;
    else
        return 3;
}
