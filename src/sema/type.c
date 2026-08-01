#include "type.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/class.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/func_decl.h"
#include "parser/template.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/lookup.h"
#include "sema/scope.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool MidSema_node_creates_type_name(const struct MidParser_ASTNode *node)
{
    switch (node->type) {
    case MIDPARSER_ASTNODETYPE_CLASS:
        return true;

    case MIDPARSER_ASTNODETYPE_ENUM:
        return true;

    case MIDPARSER_ASTNODETYPE_VAR_DECL_INST:
        return node->var_inst.type.squals.is_typedef;

    case MIDPARSER_ASTNODETYPE_TMPLT_PARAM:
        return node->tmplt_param.kind == MIDPARSER_TMPLTPARAM_TYPE;

    default:
        return false;
    }
}

static struct MidParser_Type
class_node_type(const struct MidParser_ASTNode *node)
{
    auto class_ = &node->class_;

    struct MidParser_Type ret = {};
    ret.spec = class_->type == MIDPARSER_CLASSTYPE_UNION
                   ? MIDPARSER_TYPESPEC_UNION
                   : MIDPARSER_TYPESPEC_CLASS;
    ret.named = class_->ident;
    MidGen_dynpush(&ret.dquals, (struct MidParser_TypeDataQual){});

    return ret;
}

struct MidParser_Type MidSema_node_type(const struct MidParser_ASTNode *node,
                                        struct MidSema_Scope *scope)
{
    if (node->type == MIDPARSER_ASTNODETYPE_VAR_DECL_INST) {
        return MidParser_copy_type(&node->var_inst.type);
    } else if (node->type == MIDPARSER_ASTNODETYPE_FUNC_DECL) {
        return MidParser_create_func_type(scope, node->func_decl.name);
    } else if (node->type == MIDPARSER_ASTNODETYPE_CLASS) {
        return class_node_type(node);
    } else if (node->type == MIDPARSER_ASTNODETYPE_TMPLT_PARAM) {
        assert(node->tmplt_param.kind == MIDPARSER_TMPLTPARAM_NONTYPE);
        return MidParser_copy_type(&node->tmplt_param.non_type.type);
    } else {
        MID_CRASH("fetching the data type of this type of node not supported");
    }
}

static void mark_expr_unknown_ret(struct MidParser_Expr *expr)
{
    expr->ret = MidParser_create_unknown_type();
}

static void typecheck_strlit_expr(struct MidParser_Expr *expr)
{
    // str literals are of type const char[]
    enum MidParser_TypeSpec elem_spec;
    switch (expr->type) {
    case MIDPARSER_EXPRTYPE_STRING_LIT:
        elem_spec = MIDPARSER_TYPESPEC_CHAR;
        break;

    case MIDPARSER_EXPRTYPE_WSTRING_LIT:
        elem_spec = MIDPARSER_TYPESPEC_WCHAR;
        break;

    case MIDPARSER_EXPRTYPE_STRING16_LIT:
        elem_spec = MIDPARSER_TYPESPEC_CHAR16;
        break;

    case MIDPARSER_EXPRTYPE_STRING32_LIT:
        elem_spec = MIDPARSER_TYPESPEC_CHAR32;
        break;

    default:
        MID_CRASH("expr isn't a str lit");
    }

    expr->ret.spec = MIDPARSER_TYPESPEC_ARRAY;
    expr->ret.dquals.arr[0].is_const = true;

    expr->ret.array = malloc(sizeof(*expr->ret.array));
    // account for '\0'
    expr->ret.array->len = MidLit_strlit_len(&expr->info.val.str) + 1;
    expr->ret.array->elem = (struct MidParser_Type){.spec = elem_spec};
    MidGen_dynpush(&expr->ret.array->elem.dquals,
                   (struct MidParser_TypeDataQual){.is_const = true});
}

static void typecheck_lit_expr(struct MidParser_Expr *expr)
{
    if (expr->type == MIDPARSER_EXPRTYPE_STRING_LIT ||
        expr->type == MIDPARSER_EXPRTYPE_WSTRING_LIT ||
        expr->type == MIDPARSER_EXPRTYPE_STRING16_LIT ||
        expr->type == MIDPARSER_EXPRTYPE_STRING32_LIT)
        expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;
    else
        expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;

    MidGen_dynpush(&expr->ret.dquals, (struct MidParser_TypeDataQual){});

    switch (expr->type) {
    case MIDPARSER_EXPRTYPE_CHAR_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_CHAR;
        break;

    case MIDPARSER_EXPRTYPE_WCHAR_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_WCHAR;
        break;

    case MIDPARSER_EXPRTYPE_CHAR16_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_CHAR16;
        break;

    case MIDPARSER_EXPRTYPE_CHAR32_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_CHAR32;
        break;

    case MIDPARSER_EXPRTYPE_STRING_LIT:
    case MIDPARSER_EXPRTYPE_WSTRING_LIT:
    case MIDPARSER_EXPRTYPE_STRING16_LIT:
    case MIDPARSER_EXPRTYPE_STRING32_LIT:
        typecheck_strlit_expr(expr);
        break;

    case MIDPARSER_EXPRTYPE_INT_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_INT;
        break;
    case MIDPARSER_EXPRTYPE_UINT_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_UINT;
        break;

    case MIDPARSER_EXPRTYPE_LONG_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_LONG;
        break;
    case MIDPARSER_EXPRTYPE_ULONG_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_ULONG;
        break;

    case MIDPARSER_EXPRTYPE_LONGLONG_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_LONGLONG;
        break;
    case MIDPARSER_EXPRTYPE_ULONGLONG_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_ULONGLONG;
        break;

    case MIDPARSER_EXPRTYPE_FLOAT_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_FLOAT;
        break;

    case MIDPARSER_EXPRTYPE_DOUBLE_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_DOUBLE;
        break;

    case MIDPARSER_EXPRTYPE_LONGDOUBLE_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_LONGDOUBLE;
        break;

    case MIDPARSER_EXPRTYPE_BOOL_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_BOOL;
        break;

    case MIDPARSER_EXPRTYPE_NULLPTR_LIT:
        expr->ret.spec = MIDPARSER_TYPESPEC_NULLPTR;
        break;

    default:
        MID_CRASH("expr isn't a literal");
    }
}

static struct MidDiag_Diag bad_ctor_call_type(const struct MidParser_Type *type,
                                              const struct MidLexer_Token *tok)
{
    char *str = MidParser_type_to_str(type);

    struct MidDiag_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg =
            MidPrint_fmt_to_str("can not call constructor on type '%s'", str),
        .err = MIDDIAG_ERR_BAD_IDENTIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };

    free(str);
    return ret;
}

static void typecheck_ident_expr(struct MidParser_Expr *expr,
                                 struct MidSema_Scope *scope,
                                 struct MidDiag_DiagVec *diags)
{
    assert(expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER);

    auto ident = MidSema_find_ident_const(scope, expr->tok->ident);
    if (!ident) {
        MidGen_dynpush(diags, MidDiag_ident_undeclared_err(
                                  expr->tok->ident, expr->tok,
                                  MIDDIAG_ERR_UNDECLARED_IDENTIFIER));
        expr->ret = MidParser_toktype_to_type(MIDLEXER_TOKENTYPE_INT);
        return;
    }
    assert(ident->decl);

    // template non-type parameters are prvalues, all other identifiers are
    // lvalues
    expr->valtype = ident->decl->type == MIDPARSER_ASTNODETYPE_TMPLT_PARAM
                        ? MIDPARSER_EXPRVALUE_PRVALUE
                        : MIDPARSER_EXPRVALUE_LVALUE;

    if (MidSema_node_creates_type_name(ident->decl)) {
        // functional cast stuff
        // example: ClassName(1, 2, 3)
        auto type = MidSema_type_name_type(scope, expr->tok->ident);

        expr->ret = MidParser_toktype_to_type(MIDLEXER_TOKENTYPE_INT);
        if (type.spec != MIDPARSER_TYPESPEC_CLASS &&
            type.spec != MIDPARSER_TYPESPEC_UNION) {
            MidGen_dynpush(diags, bad_ctor_call_type(&type, expr->tok));
        } else {
            expr->ret.spec = MIDPARSER_TYPESPEC_FUNC;
            expr->ret.func.is_tor = true;
            expr->ret.func.scope = scope;
            expr->ret.func.name = MidSema_deref_identptr(&type.named)->name;
        }

        MidParser_Type_deinit(&type);
    } else {
        assert(MidSema_ident_type(scope, ident, &expr->ret));
    }
}

static struct MidDiag_Diag
this_outside_nonstatic_method_err(const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str(
            "can't use 'this' outside a non-static member function"),
        .err = MIDDIAG_ERR_BAD_THIS_USAGE,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_this_expr(struct MidParser_Expr *expr,
                                const struct MidSema_Scope *scope,
                                struct MidDiag_DiagVec *diags)
{
    assert(expr->type == MIDPARSER_EXPRTYPE_THIS);

    // "this" is a prvalue
    expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;

    auto func_scope =
        MidSema_closest_scope_of_type_const(scope, MIDSEMA_SCOPETYPE_FUNC);
    if (!func_scope) {
        MidGen_dynpush(diags, this_outside_nonstatic_method_err(expr->tok));
        goto invalid_this;
    }

    const struct MidParser_FuncDecl *func = &func_scope->node->func_decl;
    if (!MidParser_func_is_method(func) || func->ret.squals.is_static) {
        MidGen_dynpush(diags, this_outside_nonstatic_method_err(expr->tok));
        goto invalid_this;
    }

    assert(MidParser_func_parent(func)->type == MIDSEMA_SCOPETYPE_CLASS);
    const struct MidParser_Class *class_ =
        &MidParser_func_parent(func)->node->class_;

    expr->ret.spec = class_->type == MIDPARSER_CLASSTYPE_UNION
                         ? MIDPARSER_TYPESPEC_UNION
                         : MIDPARSER_TYPESPEC_CLASS;
    expr->ret.named = class_->ident;

    MidGen_dynpush(&expr->ret.dquals,
                   ((struct MidParser_TypeDataQual){
                       .is_const = func->quals.is_const,
                       .is_volatile = func->quals.is_volatile}));
    MidGen_dynpush(&expr->ret.dquals, ((struct MidParser_TypeDataQual){}));

    return;

invalid_this:
    // default to an int*
    expr->ret = MidParser_toktype_to_type(MIDLEXER_TOKENTYPE_INT);
    MidGen_dynpush(&expr->ret.dquals, ((struct MidParser_TypeDataQual){}));
}

// also works with member select operators:
//    var.method();
// becomes:
//    var::method();
static void scope_res_name_impl(const struct MidParser_Expr *expr,
                                struct Mid_Dynstr *str)
{
    if (expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER) {
        MidDynstr_append(str, expr->info.ident);
    } else if (expr->type == MIDPARSER_EXPRTYPE_THIS) {
        MidDynstr_append(str, "this");
    } else {
        struct MidParser_Expr *scope =
            expr->type == MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES
                ? NULL
                : &expr->info.args.arr[0];
        struct MidParser_Expr *child =
            expr->type == MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES
                ? &expr->info.args.arr[0]
                : &expr->info.args.arr[1];

        if (scope && scope->type == MIDPARSER_EXPRTYPE_IDENTIFIER)
            MidDynstr_append(str, scope->info.ident);
        else if (scope && scope->type == MIDPARSER_EXPRTYPE_THIS)
            MidDynstr_append(str, "this");
        MidDynstr_append(str, "::");

        scope_res_name_impl(child, str);
    }
}

static char *scope_res_name(const struct MidParser_Expr *expr)
{
    struct Mid_Dynstr ret = {};
    scope_res_name_impl(expr, &ret);
    return ret.str;
}

/*
static const char *scope_res_ident(const struct MidParser_Expr *expr)
{
    if (expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER)
        return expr->info.ident;
    else if (expr->type == MIDPARSER_EXPRTYPE_BIN_SCOPE_RES)
        return scope_res_ident(&expr->info.args.arr[1]);
    else if (expr->type == MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES)
        return scope_res_ident(&expr->info.args.arr[0]);
    else if (MidParser_is_memb_sel(expr->type))
        return scope_res_ident(&expr->info.args.arr[1]);
    else
        MID_CRASH("expr is not a scope resolution");
}
*/

static struct MidDiag_Diag
bad_overload_call_err(const char *name, const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg =
            MidPrint_fmt_to_str("call to nonexistent overload of '%s'", name),
        .err = MIDDIAG_ERR_BAD_IDENTIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static const struct MidParser_TypeDataQual *
method_call_this_quals(struct MidParser_Expr *call)
{
    const struct MidParser_Expr *lhs = &call->info.args.arr[0];

    const struct MidParser_TypeDataQualVec *quals =
        &lhs->info.args.arr[0].ret.dquals;
    return &quals->arr[quals->len - 1];
}

static struct MidDiag_Diag
note_func_candidate(const struct MidParser_FuncDecl *func)
{
    return (struct MidDiag_Diag){
        .pos = MIDPARSER_GET_START(func)->pos,
        .line = MIDPARSER_GET_START(func)->line,
        .msg = MidPrint_fmt_to_str("candidate not viable"),
        .type = MIDDIAG_TYPE_NOTE,
    };
}

static void note_func_candidates(const char *name,
                                 const struct MidParser_Expr *args,
                                 mid_isize n_args, struct MidSema_Scope *scope,
                                 bool is_qualified,
                                 struct MidDiag_DiagVec *diags)
{
    auto cands =
        MidSema_find_candidate_funcs(name, args, n_args, scope, is_qualified);

    for (mid_isize i = 0; i < cands.len; ++i) {
        MidGen_dynpush(diags, note_func_candidate(cands.arr[i]));
    }

    MidGen_dyndeinit(&cands);
}

static void note_method_candidates(const char *name,

                                   struct MidSema_Scope *scope,
                                   struct MidDiag_DiagVec *diags)
{
    auto cands = MidSema_find_candidate_methods(name, scope);

    for (mid_isize i = 0; i < cands.len; ++i) {
        MidGen_dynpush(diags, note_func_candidate(cands.arr[i]));
    }

    MidGen_dyndeinit(&cands);
}

static void set_func_call_node(struct MidParser_Expr *expr,
                               struct MidSema_Scope *scope,
                               struct MidDiag_DiagVec *diags)
{
    const struct MidParser_Expr *lhs = &expr->info.args.arr[0];

    bool qualified =
        MidParser_is_scope_res(lhs->type) || MidParser_is_memb_sel(lhs->type);
    char *qual_name = scope_res_name(lhs);

    struct MidSema_Scope *res =
        lhs->ret.spec == MIDPARSER_TYPESPEC_FUNC ? lhs->ret.func.scope : scope;

    if (lhs->ret.spec == MIDPARSER_TYPESPEC_FUNC) {
        // bum ass code
        bool is_method = MidParser_is_memb_sel(lhs->type);

        if (is_method)
            expr->node = MIDPARSER_GET_NODE(MidSema_find_method(
                lhs->ret.func.name, &expr->info.args.arr[1],
                expr->info.args.len - 1, res, method_call_this_quals(expr)));
        else
            expr->node = MIDPARSER_GET_NODE(
                MidSema_find_func(lhs->ret.func.name, &expr->info.args.arr[1],
                                  expr->info.args.len - 1, res, qualified));

        if (!expr->node) {
            MidGen_dynpush(diags, bad_overload_call_err(qual_name, lhs->tok));
            if (is_method)
                note_method_candidates(lhs->ret.func.name, res, diags);
            else
                note_func_candidates(
                    lhs->ret.func.name, &expr->info.args.arr[1],
                    expr->info.args.len - 1, res, qualified, diags);
        } else {
            printf("calling func at %d:%d\n", expr->node->start->pos.line,
                   expr->node->start->pos.column);
        }
    } else if (lhs->ret.spec == MIDPARSER_TYPESPEC_FPTR) {
        MID_CRASH("calling function ptrs not implemented");
    } else {
        MidGen_dynpush(diags, MidDiag_func_undeclared_err(
                                  lhs->info.ident, lhs->tok,
                                  MIDDIAG_ERR_UNDECLARED_FUNCTION));
    }

    free(qual_name);
}

static void typecheck_call_expr(struct MidParser_Expr *expr,
                                struct MidSema_Scope *scope,
                                struct MidDiag_DiagVec *diags)
{
    set_func_call_node(expr, scope, diags);
    if (!expr->node)
        return;

    expr->ret = MidParser_copy_type(&expr->node->func_decl.ret);

    if (expr->node->func_decl.ret.lv_ref)
        expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;
    else if (expr->node->func_decl.ret.rv_ref)
        expr->valtype = MIDPARSER_EXPRVALUE_XVALUE;
    else
        expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;
}

static void typecheck_assignment_expr(struct MidParser_Expr *expr,
                                      struct MidDiag_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];

    if (lhs->valtype != MIDPARSER_EXPRVALUE_LVALUE)
        MidGen_dynpush(diags, ((struct MidDiag_Diag){
                                  .pos = expr->tok->pos,
                                  .line = expr->tok->line,
                                  .msg = MidPrint_fmt_to_str(
                                      "can't assign to %s",
                                      lhs->valtype == MIDPARSER_EXPRVALUE_XVALUE
                                          ? "an xvalue"
                                          : "a prvalue"),
                                  .err = MIDDIAG_ERR_BAD_ASSIGNMENT,
                                  .type = MIDDIAG_TYPE_ERROR,
                              }));

    expr->ret = MidParser_copy_type(&lhs->ret);
}

static void typecheck_inc_dec_expr(struct MidParser_Expr *expr,
                                   struct MidDiag_DiagVec *diags)
{
    bool is_prefix = expr->type == MIDPARSER_EXPRTYPE_PREFIX_INC ||
                     expr->type == MIDPARSER_EXPRTYPE_PREFIX_DEC;
    bool is_inc = expr->type == MIDPARSER_EXPRTYPE_PREFIX_INC ||
                  expr->type == MIDPARSER_EXPRTYPE_POSTFIX_INC;

    expr->valtype =
        is_prefix ? MIDPARSER_EXPRVALUE_LVALUE : MIDPARSER_EXPRVALUE_PRVALUE;

    if (expr->info.args.arr[0].valtype != MIDPARSER_EXPRVALUE_LVALUE)
        MidGen_dynpush(
            diags,
            ((struct MidDiag_Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = MidPrint_fmt_to_str("%s %s requires an lvalue",
                                           is_prefix ? "prefix" : "postfix",
                                           is_inc ? "increment" : "decrement"),
                .err = MIDDIAG_ERR_BAD_ASSIGNMENT,
                .type = MIDDIAG_TYPE_ERROR,
            }));

    expr->ret = MidParser_copy_type(&expr->info.args.arr[0].ret);
}

static void typecheck_deref_expr(struct MidParser_Expr *expr,
                                 struct MidDiag_DiagVec *diags)
{
    expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;

    auto arg = &expr->info.args.arr[0];

    if (MidParser_n_indir(&arg->ret) == 0) {
        char *tname = MidParser_type_to_str(&arg->ret);
        MidGen_dynpush(diags, ((struct MidDiag_Diag){
                                  .pos = expr->tok->pos,
                                  .line = expr->tok->line,
                                  .msg = MidPrint_fmt_to_str(
                                      "cannot dereference type '%s'", tname),
                                  .err = MIDDIAG_ERR_BAD_DEREF,
                                  .type = MIDDIAG_TYPE_ERROR,
                              }));
        free(tname);
        expr->ret = MidParser_copy_type(&arg->ret);
    } else {
        bool failed;
        expr->ret = MidParser_deref_type(&arg->ret, &failed);
        assert(!failed);
    }
}

static void typecheck_ref_expr(struct MidParser_Expr *expr,
                               struct MidDiag_DiagVec *diags)
{
    expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;

    if (expr->info.args.arr[0].valtype != MIDPARSER_EXPRVALUE_LVALUE)
        MidGen_dynpush(
            diags, ((struct MidDiag_Diag){
                       .pos = expr->tok->pos,
                       .line = expr->tok->line,
                       .msg = MidPrint_fmt_to_str("cannot reference rvalue"),
                       .err = MIDDIAG_ERR_BAD_REF,
                       .type = MIDDIAG_TYPE_ERROR,
                   }));

    bool failed;
    expr->ret = MidParser_ref_type(&expr->info.args.arr[0].ret, &failed);
    assert(!failed);
}

static void typecheck_arr_subscr_expr(struct MidParser_Expr *expr,
                                      struct MidDiag_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_valid = lhs->ret.spec == MIDPARSER_TYPESPEC_ARRAY ||
                     MidParser_n_indir(&lhs->ret) > 0;
    bool rhs_valid = rhs->ret.spec == MIDPARSER_TYPESPEC_ARRAY ||
                     MidParser_n_indir(&rhs->ret) > 0;

    bool lhs_int = MidParser_is_integral_typespec(lhs->ret.spec) &&
                   MidParser_n_indir(&lhs->ret) == 0;
    bool rhs_int = MidParser_is_integral_typespec(rhs->ret.spec) &&
                   MidParser_n_indir(&rhs->ret) == 0;

    if (!(lhs_valid && lhs_int) && !(rhs_valid && rhs_int)) {
        char *lhs_tname = MidParser_type_to_str(&lhs->ret);
        char *rhs_tname = MidParser_type_to_str(&lhs->ret);
        MidGen_dynpush(diags, ((struct MidDiag_Diag){
                                  .pos = expr->tok->pos,
                                  .line = expr->tok->line,
                                  .msg = MidPrint_fmt_to_str(
                                      "cannot subscript types '%s' and '%s'",
                                      lhs_tname, rhs_tname),
                                  .err = MIDDIAG_ERR_BAD_ARRAY_SUBSCRIPT,
                                  .type = MIDDIAG_TYPE_ERROR,
                              }));
        free(lhs_tname);
        free(rhs_tname);
    } else if ((lhs_valid && lhs->valtype == MIDPARSER_EXPRVALUE_LVALUE) ||
               (rhs_valid && rhs->valtype == MIDPARSER_EXPRVALUE_LVALUE) ||
               MidParser_n_indir(&lhs->ret) > 0 ||
               MidParser_n_indir(&rhs->ret) > 0) {
        expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;
    } else {
        expr->valtype = MIDPARSER_EXPRVALUE_XVALUE;
    }
}

static void typecheck_comma_expr(struct MidParser_Expr *expr)
{
    auto rhs = &expr->info.args.arr[1];

    expr->valtype = rhs->valtype;
    expr->ret = MidParser_copy_type(&rhs->ret);
}

static struct MidDiag_Diag
cond_one_result_void_err(const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = strdup("only one result of conditional '?' expression is void"),
        .err = MIDDIAG_ERR_BAD_CONDITIONAL,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_conditional_expr(struct MidParser_Expr *expr,
                                       struct MidDiag_DiagVec *diags)
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
        e2->ret.spec == MIDPARSER_TYPESPEC_VOID && MidParser_n_indir(&e2->ret) == 0;
    bool e3_void =
        e3->ret.spec == MIDPARSER_TYPESPEC_VOID && MidParser_n_indir(&e3->ret) == 0;

    if (e2_void && e3_void) {
        expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;
        expr->ret = MidParser_copy_type(&e2->ret);
    } else if (e2_void) {
        if (e3->type != MIDPARSER_EXPRTYPE_THROW)
            MidGen_dynpush(diags, cond_one_result_void_err(e3->tok));
        expr->valtype = e3->valtype;
        expr->ret = MidParser_copy_type(&e3->ret);
    } else if (e3_void) {
        if (e2->type != MIDPARSER_EXPRTYPE_THROW)
            MidGen_dynpush(diags, cond_one_result_void_err(e2->tok));
        expr->valtype = e2->valtype;
        expr->ret = MidParser_copy_type(&e2->ret);
    }
#endif
}

static struct MidDiag_Diag bad_operands(const struct MidParser_Expr *expr,
                                        const char *type,
                                        enum MidDiag_ErrT err_type)
{
    bool unary = expr->info.args.len == 1;

    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    char *lhs_tname = MidParser_type_to_str(&lhs->ret);
    char *rhs_tname = unary ? NULL : MidParser_type_to_str(&rhs->ret);
    struct MidDiag_Diag ret;
    if (unary) {
        ret = (struct MidDiag_Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg = MidPrint_fmt_to_str("%s operator can not operate on '%s'",
                                       type, lhs_tname),
            .err = err_type,
            .type = MIDDIAG_TYPE_ERROR,
        };
    } else {
        ret = (struct MidDiag_Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg = MidPrint_fmt_to_str(
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

static enum MidParser_TypeSpec op_prom_typespec(enum MidParser_TypeSpec spec)
{
    if (MidParser_is_integral_typespec(spec))
        return MidParser_integral_prom(spec);
    else if (MidParser_is_floating_typespec(spec))
        return spec;
    else {
        /*
        *(volatile int *)NULL = 10;
        printf("spec = %d, %d\n", spec, MIDPARSER_TYPESPEC_ARRAY);
        */
        MID_CRASH("can't promote type spec");
    }
}

static struct MidParser_Type op_prom_type(struct MidParser_Type *type)
{
    auto ret = MidParser_copy_type(type);
    ret.spec = op_prom_typespec(ret.spec);
    return ret;
}

static bool is_ptr_arith_op(enum MidParser_ExprType op)
{
    return op == MIDPARSER_EXPRTYPE_ADD || op == MIDPARSER_EXPRTYPE_SUB;
}

static void typecheck_arith_bin_op_expr(struct MidParser_Expr *expr,
                                        struct MidDiag_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_ptr = MidParser_n_indir(&lhs->ret) > 0;
    bool rhs_ptr = MidParser_n_indir(&rhs->ret) > 0;

    bool bad_op_types;
    if (is_ptr_arith_op(expr->type)) {
        if (lhs_ptr && rhs_ptr)
            bad_op_types = true;
        else if (lhs_ptr)
            bad_op_types = !MidParser_is_integral_typespec(rhs->ret.spec);
        else if (rhs_ptr)
            bad_op_types = MidParser_is_integral_typespec(lhs->ret.spec);
        else
            bad_op_types = false;
    } else {
        bad_op_types = lhs_ptr || rhs_ptr;
    }

    if (bad_op_types) {
        MidGen_dynpush(diags, bad_operands(expr, "arithmetic",
                                           MIDDIAG_ERR_BAD_ARITHMETIC_OP));
        expr->ret = MidParser_copy_type(&lhs->ret);
    } else if (lhs_ptr) {
        expr->ret = MidParser_copy_type(&lhs->ret);
    } else if (rhs_ptr) {
        expr->ret = MidParser_copy_type(&rhs->ret);
    } else {
        i32 lhs_rank =
            MidParser_typespec_conv_rank(op_prom_typespec(lhs->ret.spec));
        i32 rhs_rank =
            MidParser_typespec_conv_rank(op_prom_typespec(rhs->ret.spec));
        expr->ret = lhs_rank > rhs_rank ? op_prom_type(&lhs->ret)
                                        : op_prom_type(&rhs->ret);
    }
}

static void typecheck_arith_unary_op_expr(struct MidParser_Expr *expr,
                                          struct MidDiag_DiagVec *diags)
{
    auto arg = &expr->info.args.arr[0];
    expr->ret = op_prom_type(&arg->ret);

    bool arg_ptr = MidParser_n_indir(&arg->ret) > 0;

    bool bad_op_types = arg_ptr;

    if (bad_op_types) {
        MidGen_dynpush(diags, bad_operands(expr, "arithmetic",
                                           MIDDIAG_ERR_BAD_ARITHMETIC_OP));
    }
}

static void typecheck_arith_op_expr(struct MidParser_Expr *expr,
                                    struct MidDiag_DiagVec *diags)
{
    expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;

    if (MidParser_is_unaryop(expr->type))
        typecheck_arith_unary_op_expr(expr, diags);
    else
        typecheck_arith_bin_op_expr(expr, diags);
}

// TODO: implement these
static void typecheck_logical_unary_op_expr(struct MidParser_Expr *expr,
                                            struct MidDiag_DiagVec *diags)
{
    (void)expr;
    (void)diags;
}

static void typecheck_logical_bin_op_expr(struct MidParser_Expr *expr,
                                          struct MidDiag_DiagVec *diags)
{
    (void)expr;
    (void)diags;
}

static void typecheck_logical_op_expr(struct MidParser_Expr *expr,
                                      struct MidDiag_DiagVec *diags)
{
    MID_CRASH("typechecking logical op exprs hasn't been implemented yet");

    expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;
    expr->ret = MidParser_toktype_to_type(MIDLEXER_TOKENTYPE_BOOL);

    if (MidParser_is_unaryop(expr->type))
        typecheck_logical_unary_op_expr(expr, diags);
    else
        typecheck_logical_bin_op_expr(expr, diags);
}

static void typecheck_comp_op_expr(struct MidParser_Expr *expr,
                                   struct MidDiag_DiagVec *diags)
{
    expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;
    expr->ret = MidParser_toktype_to_type(MIDLEXER_TOKENTYPE_BOOL);

    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_ptr = MidParser_n_indir(&lhs->ret) > 0;
    bool lhs_void_ptr = lhs->ret.spec == MIDPARSER_TYPESPEC_VOID &&
                        MidParser_n_indir(&lhs->ret);
    bool rhs_ptr = MidParser_n_indir(&rhs->ret) > 0;
    bool rhs_void_ptr = rhs->ret.spec == MIDPARSER_TYPESPEC_VOID &&
                        MidParser_n_indir(&rhs->ret);

    bool eq = lhs->ret.spec == rhs->ret.spec && MidParser_n_indir(&lhs->ret) &&
              MidParser_n_indir(&rhs->ret);

    // an arithmetic operator can operate on primitives, ptrs of the same type,
    // or a ptr and a void ptr
    bool bad_op_types =
        (lhs_ptr || rhs_ptr) && (!eq && (!lhs_void_ptr && !rhs_void_ptr));

    if (bad_op_types) {
        MidGen_dynpush(
            diags, bad_operands(expr, "comp", MIDDIAG_ERR_BAD_COMPARISON_OP));
    }
}

static struct MidSema_Scope *bin_scope_res_scope(struct MidParser_Expr *expr,
                                                 struct MidSema_Scope *scope)
{
    auto lhs = &expr->info.args.arr[0];
    assert(lhs->type == MIDPARSER_EXPRTYPE_IDENTIFIER);

    struct MidSema_Scope *base = MidSema_closest_rnce_scope(scope);

    const char *name = lhs->info.ident;
    struct MidSema_Scope *res = MidSema_resolve_scope(name, base);

    assert(res);
    return res;
}

static struct MidSema_Scope *unary_scope_res_scope(struct MidSema_Scope *scope)
{
    auto res = scope;
    while (res->parent)
        res = res->parent;
    return res;
}

static void typecheck_scope_res_expr(struct MidParser_Expr *expr,
                                     struct MidSema_Scope *scope,
                                     struct MidDiag_DiagVec *diags)
{
    struct MidSema_Scope *res;
    struct MidParser_Expr *arg;
    if (expr->type == MIDPARSER_EXPRTYPE_BIN_SCOPE_RES) {
        res = bin_scope_res_scope(expr, scope);
        arg = &expr->info.args.arr[1];
    } else {
        res = unary_scope_res_scope(scope);
        arg = &expr->info.args.arr[0];
    }

    MidSema_typecheck_expr(arg, res, diags);

    expr->ret = MidParser_copy_type(&arg->ret);
    expr->valtype = arg->valtype;
    expr->res_scope = MidParser_is_scope_res(arg->type) ? arg->res_scope : res;
}

static struct MidDiag_Diag
memb_sel_lhs_not_class_err(const struct MidParser_Expr *memb_sel)
{
    char *lhs_type = MidParser_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct MidDiag_Diag ret = {
        .pos = memb_sel->tok->pos,
        .line = memb_sel->tok->line,
        .msg = MidPrint_fmt_to_str(
            "member select lhs '%s' is not a class or union", lhs_type),
        .err = MIDDIAG_ERR_BAD_MEMB_SEL,
        .type = MIDDIAG_TYPE_ERROR};

    free(lhs_type);
    return ret;
}

static struct MidDiag_Diag
memb_sel_expects_ptr_err(const struct MidParser_Expr *memb_sel)
{
    char *lhs_type = MidParser_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct MidDiag_Diag ret = {
        .pos = memb_sel->tok->pos,
        .line = memb_sel->tok->line,
        .msg = MidPrint_fmt_to_str("member select lhs '%s' is not a pointer",
                                   lhs_type),
        .err = MIDDIAG_ERR_BAD_MEMB_SEL,
        .type = MIDDIAG_TYPE_ERROR};

    free(lhs_type);
    return ret;
}

static struct MidDiag_Diag
memb_sel_expects_non_ptr_err(const struct MidParser_Expr *memb_sel)
{
    char *lhs_type = MidParser_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct MidDiag_Diag ret = {
        .pos = memb_sel->tok->pos,
        .line = memb_sel->tok->line,
        .msg = MidPrint_fmt_to_str("member select lhs '%s' is a pointer",
                                   lhs_type),
        .err = MIDDIAG_ERR_BAD_MEMB_SEL,
        .type = MIDDIAG_TYPE_ERROR};

    free(lhs_type);
    return ret;
}

static bool memb_sel_expects_ptr(enum MidParser_ExprType type)
{
    return type == MIDPARSER_EXPRTYPE_PTR_MEMB_SEL ||
           type == MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
}

static struct MidDiag_Diag unknown_field_err(const char *field,
                                             const char *class_, bool is_union,
                                             const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("unknown field '%s' in %s '%s'", field,
                                   is_union ? "union" : "class", class_),
        .err = MIDDIAG_ERR_BAD_IDENTIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static void typecheck_memb_sel(struct MidParser_Expr *expr,
                               struct MidSema_Scope *scope,
                               struct MidDiag_DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    MidSema_typecheck_expr(lhs, scope, diags);
    if (!MidParser_type_is_typecheckable(&lhs->ret)) {
        mark_expr_unknown_ret(expr);
        return;
    }

    if (lhs->ret.spec != MIDPARSER_TYPESPEC_CLASS &&
        lhs->ret.spec != MIDPARSER_TYPESPEC_UNION) {
        MidGen_dynpush(diags, memb_sel_lhs_not_class_err(expr));
        return;
    } else if (memb_sel_expects_ptr(expr->type) &&
               MidParser_n_indir(&lhs->ret) != 1) {
        MidGen_dynpush(diags, memb_sel_expects_ptr_err(expr));
    } else if (!memb_sel_expects_ptr(expr->type) &&
               MidParser_n_indir(&lhs->ret) != 0) {
        MidGen_dynpush(diags, memb_sel_expects_non_ptr_err(expr));
    } else if (rhs->type != MIDPARSER_EXPRTYPE_IDENTIFIER) {
        MidGen_dynpush(
            diags, MidDiag_expected_token_err("identifier", expr->tok,
                                              MIDDIAG_ERR_MISSING_IDENTIFIER));
        return;
    }

    const struct MidParser_Class *class_ =
        &MidSema_deref_identptr(&lhs->ret.named)->decl->class_;
    const char *field_name = rhs->info.ident;
    mid_isize field_idx = MidParser_find_field(class_, field_name);
    if (field_idx == -1) {
        MidGen_dynpush(
            diags, unknown_field_err(field_name, class_->name,
                                     class_->type == MIDPARSER_CLASSTYPE_UNION,
                                     rhs->tok));
        return;
    }

    auto field = class_->childs.arr[field_idx];

    if (field->type == MIDPARSER_ASTNODETYPE_VAR_DECL) {
        expr->ret = MidParser_copy_type(
            &MidParser_decl_inst_of_name(&field->var_decl, field_name)->type);
        expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;
    } else {
        expr->ret = MidParser_create_func_type(
            MidSema_deref_identptr(&class_->ident)->class_info.def_scope,
            field->func_decl.name);
        expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;
    }
}

static void typecheck_op_expr(struct MidParser_Expr *expr,
                              struct MidSema_Scope *scope,
                              struct MidDiag_DiagVec *diags)
{
    if (expr->type == MIDPARSER_EXPRTYPE_FUNC_CALL)
        typecheck_call_expr(expr, scope, diags);
    else if (MidParser_is_assignment(expr->type))
        typecheck_assignment_expr(expr, diags);
    else if (expr->type == MIDPARSER_EXPRTYPE_PREFIX_INC ||
             expr->type == MIDPARSER_EXPRTYPE_PREFIX_DEC ||
             expr->type == MIDPARSER_EXPRTYPE_POSTFIX_INC ||
             expr->type == MIDPARSER_EXPRTYPE_POSTFIX_DEC)
        typecheck_inc_dec_expr(expr, diags);
    else if (expr->type == MIDPARSER_EXPRTYPE_DEREF)
        typecheck_deref_expr(expr, diags);
    else if (expr->type == MIDPARSER_EXPRTYPE_REF)
        typecheck_ref_expr(expr, diags);
    else if (expr->type == MIDPARSER_EXPRTYPE_ARRAY_SUBSCR)
        typecheck_arr_subscr_expr(expr, diags);
    else if (expr->type == MIDPARSER_EXPRTYPE_COMMA)
        typecheck_comma_expr(expr);
    else if (expr->type == MIDPARSER_EXPRTYPE_CONDITIONAL)
        typecheck_conditional_expr(expr, diags);
    else if (MidParser_is_arith_op(expr->type))
        typecheck_arith_op_expr(expr, diags);
    else if (MidParser_is_logical_op(expr->type))
        typecheck_logical_op_expr(expr, diags);
    else if (MidParser_is_comp_op(expr->type))
        typecheck_comp_op_expr(expr, diags);
    else if (MidParser_is_scope_res(expr->type))
        typecheck_scope_res_expr(expr, scope, diags);
    else if (MidParser_is_memb_sel(expr->type))
        typecheck_memb_sel(expr, scope, diags);
    else {
        printf("op at %d:%d\n", expr->tok->pos.line, expr->tok->pos.column);
        printf("op type = %d\n", expr->type);
        MID_CRASH("typechecking op not implemented");
    }
}

static void typecheck_overloaded_op(struct MidParser_Expr *expr,
                                    struct MidParser_FuncDecl *overload)
{
    printf("found op overload at %d:%d\n", expr->tok->pos.line,
           expr->tok->pos.column);
    printf("op overload decl at %d:%d\n",
           MIDPARSER_GET_START(overload)->pos.line,
           MIDPARSER_GET_START(overload)->pos.column);

    expr->overloaded = true;
    expr->node = MIDPARSER_GET_NODE(overload);
    expr->ret = MidParser_copy_type(&overload->ret);

    if (overload->ret.lv_ref)
        expr->valtype = MIDPARSER_EXPRVALUE_LVALUE;
    else if (overload->ret.rv_ref)
        expr->valtype = MIDPARSER_EXPRVALUE_XVALUE;
    else
        expr->valtype = MIDPARSER_EXPRVALUE_PRVALUE;
}

static bool has_no_untypecheckable_args(struct MidParser_Expr *expr)
{
    for (mid_isize i = 0; i < expr->info.args.len; ++i) {
        if (!MidParser_type_is_typecheckable(&expr->info.args.arr[i].ret))
            return false;
    }

    return true;
}

void MidSema_typecheck_expr(struct MidParser_Expr *expr,
                            struct MidSema_Scope *scope,
                            struct MidDiag_DiagVec *diags)
{
    if (expr->typechecked)
        return;
    expr->typechecked = true;

    if (MidParser_is_numlit(expr->type)) {
        typecheck_lit_expr(expr);
    } else if (expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER) {
        typecheck_ident_expr(expr, scope, diags);
    } else if (expr->type == MIDPARSER_EXPRTYPE_THIS) {
        typecheck_this_expr(expr, scope, diags);
    } else {
        // some operators are weird
        bool typecheck_args = !MidParser_is_scope_res(expr->type) &&
                              !MidParser_is_memb_sel(expr->type);
        if (typecheck_args) {
            for (mid_isize i = 0; i < expr->info.args.len; ++i)
                MidSema_typecheck_expr(&expr->info.args.arr[i], scope, diags);

            if (!has_no_untypecheckable_args(expr)) {
                mark_expr_unknown_ret(expr);
                return;
            }
        }

        struct MidParser_FuncDecl *overload = MidSema_find_op_overload(
            expr->type, expr->info.args.arr, expr->info.args.len, scope);
        if (!overload)
            typecheck_op_expr(expr, scope, diags);
        else
            typecheck_overloaded_op(expr, overload);
    }
}

static struct MidDiag_Diag
no_matching_ctor_err(const struct MidParser_Type *type,
                     const struct MidLexer_Token *tok)
{
    char *str = MidParser_type_to_str(type);

    struct MidDiag_Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("no matching constructor for '%s'", str),
        .err = MIDDIAG_ERR_NO_MATCHING_CTOR,
        .type = MIDDIAG_TYPE_ERROR,
    };

    free(str);
    return ret;
}

// returns whether or not the ctor is correct
static bool typecheck_vdecl_class_type_ctor(struct MidParser_VarDeclInst *inst)
{
    assert(inst->has_ctor);

    auto ident = MidSema_deref_identptr(&inst->type.named);
    inst->ctor.ctor =
        MidSema_find_func(ident->name, inst->ctor.args.arr, inst->ctor.args.len,
                          ident->class_info.def_scope, true);

    return inst->ctor.ctor != NULL;
}

// returns whether or not the ctor is correct
static bool
typecheck_vdecl_generic_type_ctor(struct MidParser_VarDeclInst *inst)
{
    assert(inst->has_ctor);

    if (inst->ctor.args.len > 1)
        return false;

    auto arg = &inst->ctor.args.arr[0];
    return MidSema_can_convert(&arg->ret, arg->valtype, &inst->type);
}

void MidSema_typecheck_var_decl_inst(struct MidParser_VarDeclInst *inst,
                                     struct MidDiag_DiagVec *diags)
{
    inst->typechecked = true;
    if (!inst->has_ctor)
        return;

    bool bad;
    if ((inst->type.spec == MIDPARSER_TYPESPEC_CLASS ||
         inst->type.spec == MIDPARSER_TYPESPEC_UNION) &&
        MidParser_n_indir(&inst->type) == 0) {

        bad = !typecheck_vdecl_class_type_ctor(inst);
    } else {
        bad = !typecheck_vdecl_generic_type_ctor(inst);
    }

    if (bad)
        MidGen_dynpush(diags, no_matching_ctor_err(&inst->type,
                                                   MIDPARSER_GET_START(inst)));
}

static struct MidDiag_Diag
invalid_return_stmt_type_err(const struct MidParser_Type *func_type,
                             const struct MidParser_Type *ret_type,
                             const struct MidLexer_Token *tok)
{
    char *func_type_str = MidParser_type_to_str(func_type);

    struct MidDiag_Diag ret;

    if (ret_type) {
        char *ret_type_str = MidParser_type_to_str(ret_type);

        ret = (struct MidDiag_Diag){
            .pos = tok->pos,
            .line = tok->line,
            .msg =
                MidPrint_fmt_to_str("returning '%s' in function of type '%s'",
                                    ret_type_str, func_type_str),
            .err = MIDDIAG_ERR_BAD_RETURN_STMT_TYPE,
            .type = MIDDIAG_TYPE_ERROR,
        };

        free(ret_type_str);
    } else {
        ret = (struct MidDiag_Diag){
            .pos = tok->pos,
            .line = tok->line,
            .msg = MidPrint_fmt_to_str(
                "expected a return value in function of type '%s'",
                func_type_str),
            .err = MIDDIAG_ERR_BAD_RETURN_STMT_TYPE,
            .type = MIDDIAG_TYPE_ERROR,
        };
    }

    free(func_type_str);

    return ret;
}

static struct MidDiag_Diag
return_outside_func_err(const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("return statement outside a function"),
        .err = MIDDIAG_ERR_RETURN_OUTSIDE_FUNC,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

void MidSema_typecheck_return(struct MidParser_Return *self,
                              const struct MidSema_Scope *scope,
                              struct MidDiag_DiagVec *diags)
{
    auto func_scope =
        MidSema_closest_scope_of_type_const(scope, MIDSEMA_SCOPETYPE_FUNC);
    if (!func_scope) {
        MidGen_dynpush(diags,
                       return_outside_func_err(MIDPARSER_GET_START(self)));
        return;
    }

    const struct MidParser_Type *func_type = &func_scope->node->func_decl.ret;
    if (!MidParser_type_is_typecheckable(func_type))
        return;

    bool is_void = func_type->spec == MIDPARSER_TYPESPEC_VOID &&
                   MidParser_n_indir(func_type) == 0;

    if (self->expr) {
        if (!MidParser_type_is_typecheckable(&self->expr->ret))
            return;
        if (!MidSema_can_convert(&self->expr->ret, self->expr->valtype,
                                 func_type))
            MidGen_dynpush(
                diags, invalid_return_stmt_type_err(func_type, &self->expr->ret,
                                                    MIDPARSER_GET_START(self)));
    } else if (!is_void) {
        MidGen_dynpush(diags, invalid_return_stmt_type_err(
                                  func_type, NULL, MIDPARSER_GET_START(self)));
    }
}

static bool is_valid_array_to_ptr(const struct MidParser_Type *src,
                                  const struct MidParser_Type *dest)
{
    if (src->spec != MIDPARSER_TYPESPEC_ARRAY)
        return false;

    // an array to ptr conversion becomes a prvalue so it can't be passed to a
    // non-const lvalue reference
    if (dest->lv_ref && !dest->dquals.arr[0].is_const)
        return false;

    mid_isize src_indir = MidParser_n_indir(src);
    mid_isize elem_indir = MidParser_n_indir(&src->array->elem);
    mid_isize dest_indir = MidParser_n_indir(dest);

    if (src_indir != 0 || elem_indir + 1 != dest_indir)
        return false;
    else if (src->array->elem.spec != dest->spec)
        return false;
    else if (!MidParser_dquals_same(src->array->elem.dquals.arr,
                                    src->array->elem.dquals.len,
                                    &dest->dquals.arr[1], dest->dquals.len - 1))
        return false;

    return true;
}

bool MidSema_can_convert(const struct MidParser_Type *src,
                         enum MidParser_ExprValueType src_valtype,
                         const struct MidParser_Type *dest)
{
    // rv references cannot take lvalues and non-const lv rereferences
    // cannot take rvalues
    if ((dest->rv_ref && !MidParser_is_rvalue(src_valtype)) ||
        (dest->lv_ref && !dest->dquals.arr[0].is_const &&
         MidParser_is_rvalue(src_valtype)))
        return false;

    if (MidParser_is_fundamental_type(src) &&
        MidParser_is_fundamental_type(dest))
        return true;
    else if (MidParser_n_indir(src) == MidParser_n_indir(dest) &&
             src->spec == dest->spec)
        return true;
    else if (MidParser_n_indir(src) > 0 && MidParser_type_is_void_ptr(dest))
        return true;
    else if (MidParser_type_is_nullptr_t(src) && MidParser_n_indir(dest) > 0)
        return true;
    else if (is_valid_array_to_ptr(src, dest))
        return true;

    return false;
}

int MidSema_conversion_rank(const struct MidParser_Type *src,
                            const struct MidParser_Type *dest)
{
    bool same_indir = MidParser_n_indir(src) == MidParser_n_indir(dest);
    bool no_indir = MidParser_n_indir(src) == 0 && MidParser_n_indir(dest) == 0;

    bool src_int = MidParser_is_integral_typespec(src->spec);
    bool dest_int = MidParser_is_integral_typespec(dest->spec);
    bool src_flt = MidParser_is_floating_typespec(src->spec);
    bool dest_flt = MidParser_is_floating_typespec(dest->spec);

    if (same_indir && src->spec == dest->spec)
        return 1;
    else if (no_indir && ((src_int && dest_int) || (src_flt && dest_flt)))
        return 2;
    else
        return 3;
}
