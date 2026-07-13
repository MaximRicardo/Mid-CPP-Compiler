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

bool Sema_node_creates_type_name(const struct Parser_ASTNode *node)
{
    switch (node->type) {
    case PARSER_ASTNODETYPE_CLASS:
        return true;

    case PARSER_ASTNODETYPE_ENUM:
        return true;

    case PARSER_ASTNODETYPE_VAR_DECL:
        return node->var_decl.insts.arr[0].type.squals.is_typedef;

    case PARSER_ASTNODETYPE_TMPLT_PARAM:
        return node->tmplt_param.kind == PARSER_TMPLTPARAM_TYPE;

    default:
        return NULL;
    }
}

static struct Parser_Type class_node_type(const struct Parser_ASTNode *node)
{
    auto class_ = &node->class_;

    struct Parser_Type ret = {};
    ret.spec = class_->type == PARSER_CLASSTYPE_UNION ? PARSER_TYPESPEC_UNION
                                                      : PARSER_TYPESPEC_CLASS;
    ret.named.parent = class_->parent;
    ret.named.ident = class_->ident_idx;
    gen_dynpush(&ret.dquals, (struct Parser_TypeDataQual){});

    return ret;
}

struct Parser_Type Sema_node_type(const struct Parser_ASTNode *node,
                                  struct Sema_Scope *scope, const char *name)
{
    if (node->type == PARSER_ASTNODETYPE_VAR_DECL) {
        auto inst = Parser_decl_inst_of_name_const(&node->var_decl, name);
        assert(inst);
        return Parser_copy_type(&inst->type);
    } else if (node->type == PARSER_ASTNODETYPE_FUNC_DECL) {
        return Parser_create_func_type(scope, node->func_decl.name);
    } else if (node->type == PARSER_ASTNODETYPE_CLASS) {
        return class_node_type(node);
    } else if (node->type == PARSER_ASTNODETYPE_TMPLT_PARAM) {
        assert(node->tmplt_param.kind == PARSER_TMPLTPARAM_NONTYPE);
        return Parser_copy_type(&node->tmplt_param.non_type.type);
    } else {
        CRASH("fetching the data type of this type of node not supported");
    }
}

static void typecheck_strlit_expr(struct Parser_Expr *expr)
{
    // str literals are of type const char[]
    enum Parser_TypeSpec elem_spec;
    switch (expr->type) {
    case PARSER_EXPRTYPE_STRING_LIT:
        elem_spec = PARSER_TYPESPEC_CHAR;
        break;

    case PARSER_EXPRTYPE_WSTRING_LIT:
        elem_spec = PARSER_TYPESPEC_WCHAR;
        break;

    case PARSER_EXPRTYPE_STRING16_LIT:
        elem_spec = PARSER_TYPESPEC_CHAR16;
        break;

    case PARSER_EXPRTYPE_STRING32_LIT:
        elem_spec = PARSER_TYPESPEC_CHAR32;
        break;

    default:
        CRASH("expr isn't a str lit");
    }

    expr->ret.spec = PARSER_TYPESPEC_ARRAY;
    expr->ret.dquals.arr[0].is_const = true;

    expr->ret.array = malloc(sizeof(*expr->ret.array));
    // account for '\0'
    expr->ret.array->len = Literal_strlit_len(&expr->info.val.str) + 1;
    expr->ret.array->elem = (struct Parser_Type){.spec = elem_spec};
    gen_dynpush(&expr->ret.array->elem.dquals,
                (struct Parser_TypeDataQual){.is_const = true});
}

static void typecheck_lit_expr(struct Parser_Expr *expr)
{
    if (expr->type == PARSER_EXPRTYPE_STRING_LIT ||
        expr->type == PARSER_EXPRTYPE_WSTRING_LIT ||
        expr->type == PARSER_EXPRTYPE_STRING16_LIT ||
        expr->type == PARSER_EXPRTYPE_STRING32_LIT)
        expr->valtype = PARSER_EXPRVALUE_LVALUE;
    else
        expr->valtype = PARSER_EXPRVALUE_PRVALUE;

    gen_dynpush(&expr->ret.dquals, (struct Parser_TypeDataQual){});

    switch (expr->type) {
    case PARSER_EXPRTYPE_CHAR_LIT:
        expr->ret.spec = PARSER_TYPESPEC_CHAR;
        break;

    case PARSER_EXPRTYPE_WCHAR_LIT:
        expr->ret.spec = PARSER_TYPESPEC_WCHAR;
        break;

    case PARSER_EXPRTYPE_CHAR16_LIT:
        expr->ret.spec = PARSER_TYPESPEC_CHAR16;
        break;

    case PARSER_EXPRTYPE_CHAR32_LIT:
        expr->ret.spec = PARSER_TYPESPEC_CHAR32;
        break;

    case PARSER_EXPRTYPE_STRING_LIT:
    case PARSER_EXPRTYPE_WSTRING_LIT:
    case PARSER_EXPRTYPE_STRING16_LIT:
    case PARSER_EXPRTYPE_STRING32_LIT:
        typecheck_strlit_expr(expr);
        break;

    case PARSER_EXPRTYPE_INT_LIT:
        expr->ret.spec = PARSER_TYPESPEC_INT;
        break;
    case PARSER_EXPRTYPE_UINT_LIT:
        expr->ret.spec = PARSER_TYPESPEC_UINT;
        break;

    case PARSER_EXPRTYPE_LONG_LIT:
        expr->ret.spec = PARSER_TYPESPEC_LONG;
        break;
    case PARSER_EXPRTYPE_ULONG_LIT:
        expr->ret.spec = PARSER_TYPESPEC_ULONG;
        break;

    case PARSER_EXPRTYPE_LONGLONG_LIT:
        expr->ret.spec = PARSER_TYPESPEC_LONGLONG;
        break;
    case PARSER_EXPRTYPE_ULONGLONG_LIT:
        expr->ret.spec = PARSER_TYPESPEC_ULONGLONG;
        break;

    case PARSER_EXPRTYPE_FLOAT_LIT:
        expr->ret.spec = PARSER_TYPESPEC_FLOAT;
        break;

    case PARSER_EXPRTYPE_DOUBLE_LIT:
        expr->ret.spec = PARSER_TYPESPEC_DOUBLE;
        break;

    case PARSER_EXPRTYPE_LONGDOUBLE_LIT:
        expr->ret.spec = PARSER_TYPESPEC_LONGDOUBLE;
        break;

    case PARSER_EXPRTYPE_BOOL_LIT:
        expr->ret.spec = PARSER_TYPESPEC_BOOL;
        break;

    case PARSER_EXPRTYPE_NULLPTR_LIT:
        expr->ret.spec = PARSER_TYPESPEC_NULLPTR;
        break;

    default:
        CRASH("expr isn't a literal");
    }
}

static struct Diag bad_ctor_call_type(const struct Parser_Type *type,
                                      const struct Lexer_Token *tok)
{
    char *str = Parser_type_to_str(type);

    struct Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("can not call constructor on type '%s'", str),
        .err = ERRORTYPE_BAD_IDENTIFIER,
        .type = DIAGTYPE_ERROR,
    };

    free(str);
    return ret;
}

static void typecheck_ident_expr(struct Parser_Expr *expr,
                                 struct Sema_Scope *scope,
                                 struct DiagVec *diags)
{
    assert(expr->type == PARSER_EXPRTYPE_IDENTIFIER);

    auto ident = Sema_find_ident_const(scope, expr->tok->ident, NULL);
    if (!ident) {
        gen_dynpush(diags,
                    Diag_ident_undeclared_err(expr->tok->ident, expr->tok,
                                              ERRORTYPE_UNDECLARED_IDENTIFIER));
        expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_INT);
    }
    assert(ident->decl);

    // template non-type parameters are prvalues, all other identifiers are
    // lvalues
    expr->valtype = ident->decl->type == PARSER_ASTNODETYPE_TMPLT_PARAM
                        ? PARSER_EXPRVALUE_PRVALUE
                        : PARSER_EXPRVALUE_LVALUE;

    if (Sema_node_creates_type_name(ident->decl)) {
        // functional cast stuff
        // example: ClassName(1, 2, 3)
        auto type = Sema_type_name_type(scope, expr->tok->ident);

        expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_INT);
        if (type.spec != PARSER_TYPESPEC_CLASS &&
            type.spec != PARSER_TYPESPEC_UNION) {
            gen_dynpush(diags, bad_ctor_call_type(&type, expr->tok));
        } else {
            expr->ret.spec = PARSER_TYPESPEC_FUNC;
            expr->ret.func.is_tor = true;
            expr->ret.func.scope = scope;
            expr->ret.func.name = Parser_named_type_ident(&type.named)->name;
        }

        Parser_Type_deinit(&type);
    } else {
        assert(Sema_ident_type(scope, ident, &expr->ret));
    }
}

static struct Diag
this_outside_nonstatic_method_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str(
            "can't use 'this' outside a non-static member function"),
        .err = ERRORTYPE_BAD_THIS_USAGE,
        .type = DIAGTYPE_ERROR,
    };
}

static void typecheck_this_expr(struct Parser_Expr *expr,
                                const struct Sema_Scope *scope,
                                struct DiagVec *diags)
{
    assert(expr->type == PARSER_EXPRTYPE_THIS);

    // "this" is a prvalue
    expr->valtype = PARSER_EXPRVALUE_PRVALUE;

    auto func_scope =
        Sema_closest_scope_of_type_const(scope, SEMA_SCOPETYPE_FUNC);
    if (!func_scope) {
        gen_dynpush(diags, this_outside_nonstatic_method_err(expr->tok));
        goto invalid_this;
    }

    const struct Parser_FuncDecl *func = &func_scope->node->func_decl;
    if (!Parser_func_is_method(func) || func->type.squals.is_static) {
        gen_dynpush(diags, this_outside_nonstatic_method_err(expr->tok));
        goto invalid_this;
    }

    assert(Parser_func_parent(func)->type == SEMA_SCOPETYPE_CLASS);
    const struct Parser_Class *class_ = &Parser_func_parent(func)->node->class_;

    expr->ret.spec = class_->type == PARSER_CLASSTYPE_UNION
                         ? PARSER_TYPESPEC_UNION
                         : PARSER_TYPESPEC_CLASS;
    expr->ret.named.parent = class_->parent;
    expr->ret.named.ident = class_->ident_idx;

    gen_dynpush(
        &expr->ret.dquals,
        ((struct Parser_TypeDataQual){.is_const = func->quals.is_const,
                                      .is_volatile = func->quals.is_volatile}));
    gen_dynpush(&expr->ret.dquals, ((struct Parser_TypeDataQual){}));

    return;

invalid_this:
    // default to an int*
    expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_INT);
    gen_dynpush(&expr->ret.dquals, ((struct Parser_TypeDataQual){}));
}

// also works with member select operators:
//    var.method();
// becomes:
//    var::method();
static void scope_res_name_impl(const struct Parser_Expr *expr,
                                struct Dynstr *str)
{
    if (expr->type == PARSER_EXPRTYPE_IDENTIFIER) {
        Dynstr_append(str, expr->info.ident);
    } else if (expr->type == PARSER_EXPRTYPE_THIS) {
        Dynstr_append(str, "this");
    } else {
        struct Parser_Expr *scope =
            expr->type == PARSER_EXPRTYPE_UNARY_SCOPE_RES
                ? NULL
                : &expr->info.args.arr[0];
        struct Parser_Expr *child =
            expr->type == PARSER_EXPRTYPE_UNARY_SCOPE_RES
                ? &expr->info.args.arr[0]
                : &expr->info.args.arr[1];

        if (scope && scope->type == PARSER_EXPRTYPE_IDENTIFIER)
            Dynstr_append(str, scope->info.ident);
        else if (scope && scope->type == PARSER_EXPRTYPE_THIS)
            Dynstr_append(str, "this");
        Dynstr_append(str, "::");

        scope_res_name_impl(child, str);
    }
}

static char *scope_res_name(const struct Parser_Expr *expr)
{
    struct Dynstr ret = {};
    scope_res_name_impl(expr, &ret);
    return ret.str;
}

/*
static const char *scope_res_ident(const struct Parser_Expr *expr)
{
    if (expr->type == PARSER_EXPRTYPE_IDENTIFIER)
        return expr->info.ident;
    else if (expr->type == PARSER_EXPRTYPE_BIN_SCOPE_RES)
        return scope_res_ident(&expr->info.args.arr[1]);
    else if (expr->type == PARSER_EXPRTYPE_UNARY_SCOPE_RES)
        return scope_res_ident(&expr->info.args.arr[0]);
    else if (Parser_is_memb_sel(expr->type))
        return scope_res_ident(&expr->info.args.arr[1]);
    else
        CRASH("expr is not a scope resolution");
}
*/

static struct Diag bad_overload_call_err(const char *name,
                                         const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("call to nonexistent overload of '%s'", name),
        .err = ERRORTYPE_BAD_IDENTIFIER,
        .type = DIAGTYPE_ERROR,
    };
}

static const struct Parser_TypeDataQual *
method_call_this_quals(struct Parser_Expr *call)
{
    const struct Parser_Expr *lhs = &call->info.args.arr[0];

    const struct Parser_TypeDataQualVec *quals =
        &lhs->info.args.arr[0].ret.dquals;
    return &quals->arr[quals->len - 1];
}

static struct Diag note_func_candidate(const struct Parser_ASTNode *func)
{
    return (struct Diag){
        .pos = func->start->pos,
        .line = func->start->line,
        .msg = Print_fmt_to_str("candidate not viable"),
        .type = DIAGTYPE_NOTE,
    };
}

static void note_func_candidates(const char *name,
                                 const struct Parser_Expr *args, isize_t n_args,
                                 struct Sema_Scope *scope, bool is_qualified,
                                 struct DiagVec *diags)
{
    auto cands =
        Sema_find_candidate_funcs(name, args, n_args, scope, is_qualified);

    for (isize_t i = 0; i < cands.len; ++i) {
        gen_dynpush(diags, note_func_candidate(cands.arr[i]));
    }

    gen_dyndeinit(&cands);
}

static void note_method_candidates(const char *name,

                                   struct Sema_Scope *scope,
                                   struct DiagVec *diags)
{
    auto cands = Sema_find_candidate_methods(name, scope);

    for (isize_t i = 0; i < cands.len; ++i) {
        gen_dynpush(diags, note_func_candidate(cands.arr[i]));
    }

    gen_dyndeinit(&cands);
}

static void set_func_call_node(struct Parser_Expr *expr,
                               struct Sema_Scope *scope, struct DiagVec *diags)
{
    const struct Parser_Expr *lhs = &expr->info.args.arr[0];

    bool qualified =
        Parser_is_scope_res(lhs->type) || Parser_is_memb_sel(lhs->type);
    char *qual_name = scope_res_name(lhs);

    struct Sema_Scope *res =
        lhs->ret.spec == PARSER_TYPESPEC_FUNC ? lhs->ret.func.scope : scope;

    if (lhs->ret.spec == PARSER_TYPESPEC_FUNC) {
        // bum ass code
        bool is_method = Parser_is_memb_sel(lhs->type);

        if (is_method)
            expr->node = Sema_find_method(
                lhs->ret.func.name, &expr->info.args.arr[1],
                expr->info.args.len - 1, res, method_call_this_quals(expr));
        else
            expr->node =
                Sema_find_func(lhs->ret.func.name, &expr->info.args.arr[1],
                               expr->info.args.len - 1, res, qualified);

        if (!expr->node) {
            gen_dynpush(diags, bad_overload_call_err(qual_name, lhs->tok));
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
    } else if (lhs->ret.spec == PARSER_TYPESPEC_FPTR) {
        CRASH("calling function ptrs not implemented");
    } else {
        gen_dynpush(diags,
                    Diag_func_undeclared_err(lhs->info.ident, lhs->tok,
                                             ERRORTYPE_UNDECLARED_FUNCTION));
    }

    free(qual_name);
}

static void typecheck_call_expr(struct Parser_Expr *expr,
                                struct Sema_Scope *scope, struct DiagVec *diags)
{
    set_func_call_node(expr, scope, diags);
    if (!expr->node)
        return;

    expr->ret = Parser_copy_type(&expr->node->func_decl.type);

    if (expr->node->func_decl.type.lv_ref)
        expr->valtype = PARSER_EXPRVALUE_LVALUE;
    else if (expr->node->func_decl.type.rv_ref)
        expr->valtype = PARSER_EXPRVALUE_XVALUE;
    else
        expr->valtype = PARSER_EXPRVALUE_PRVALUE;
}

static void typecheck_assignment_expr(struct Parser_Expr *expr,
                                      struct DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];

    if (lhs->valtype != PARSER_EXPRVALUE_LVALUE)
        gen_dynpush(
            diags,
            ((struct Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = Print_fmt_to_str("can't assign to %s",
                                        lhs->valtype == PARSER_EXPRVALUE_XVALUE
                                            ? "an xvalue"
                                            : "a prvalue"),
                .err = ERRORTYPE_BAD_ASSIGNMENT,
                .type = DIAGTYPE_ERROR,
            }));

    expr->ret = Parser_copy_type(&lhs->ret);
}

static void typecheck_inc_dec_expr(struct Parser_Expr *expr,
                                   struct DiagVec *diags)
{
    bool is_prefix = expr->type == PARSER_EXPRTYPE_PREFIX_INC ||
                     expr->type == PARSER_EXPRTYPE_PREFIX_DEC;
    bool is_inc = expr->type == PARSER_EXPRTYPE_PREFIX_INC ||
                  expr->type == PARSER_EXPRTYPE_POSTFIX_INC;

    expr->valtype =
        is_prefix ? PARSER_EXPRVALUE_LVALUE : PARSER_EXPRVALUE_PRVALUE;

    if (expr->info.args.arr[0].valtype != PARSER_EXPRVALUE_LVALUE)
        gen_dynpush(
            diags,
            ((struct Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = Print_fmt_to_str("%s %s requires an lvalue",
                                        is_prefix ? "prefix" : "postfix",
                                        is_inc ? "increment" : "decrement"),
                .err = ERRORTYPE_BAD_ASSIGNMENT,
                .type = DIAGTYPE_ERROR,
            }));

    expr->ret = Parser_copy_type(&expr->info.args.arr[0].ret);
}

static void typecheck_deref_expr(struct Parser_Expr *expr,
                                 struct DiagVec *diags)
{
    expr->valtype = PARSER_EXPRVALUE_LVALUE;

    auto arg = &expr->info.args.arr[0];

    if (Parser_n_indir(&arg->ret) == 0) {
        char *tname = Parser_type_to_str(&arg->ret);
        gen_dynpush(
            diags,
            ((struct Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = Print_fmt_to_str("cannot dereference type '%s'", tname),
                .err = ERRORTYPE_BAD_DEREF,
                .type = DIAGTYPE_ERROR,
            }));
        free(tname);
        expr->ret = Parser_copy_type(&arg->ret);
    } else {
        bool failed;
        expr->ret = Parser_deref_type(&arg->ret, &failed);
        assert(!failed);
    }
}

static void typecheck_ref_expr(struct Parser_Expr *expr, struct DiagVec *diags)
{
    expr->valtype = PARSER_EXPRVALUE_PRVALUE;

    if (expr->info.args.arr[0].valtype != PARSER_EXPRVALUE_LVALUE)
        gen_dynpush(diags,
                    ((struct Diag){
                        .pos = expr->tok->pos,
                        .line = expr->tok->line,
                        .msg = Print_fmt_to_str("cannot reference rvalue"),
                        .err = ERRORTYPE_BAD_REF,
                        .type = DIAGTYPE_ERROR,
                    }));

    bool failed;
    expr->ret = Parser_ref_type(&expr->info.args.arr[0].ret, &failed);
    assert(!failed);
}

static void typecheck_arr_subscr_expr(struct Parser_Expr *expr,
                                      struct DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_valid =
        lhs->ret.spec == PARSER_TYPESPEC_ARRAY || Parser_n_indir(&lhs->ret) > 0;
    bool rhs_valid =
        rhs->ret.spec == PARSER_TYPESPEC_ARRAY || Parser_n_indir(&rhs->ret) > 0;

    bool lhs_int = Parser_is_integral_typespec(lhs->ret.spec) &&
                   Parser_n_indir(&lhs->ret) == 0;
    bool rhs_int = Parser_is_integral_typespec(rhs->ret.spec) &&
                   Parser_n_indir(&rhs->ret) == 0;

    if (!(lhs_valid && lhs_int) && !(rhs_valid && rhs_int)) {
        char *lhs_tname = Parser_type_to_str(&lhs->ret);
        char *rhs_tname = Parser_type_to_str(&lhs->ret);
        gen_dynpush(
            diags,
            ((struct Diag){
                .pos = expr->tok->pos,
                .line = expr->tok->line,
                .msg = Print_fmt_to_str("cannot subscript types '%s' and '%s'",
                                        lhs_tname, rhs_tname),
                .err = ERRORTYPE_BAD_ARRAY_SUBSCRIPT,
                .type = DIAGTYPE_ERROR,
            }));
        free(lhs_tname);
        free(rhs_tname);
    } else if ((lhs_valid && lhs->valtype == PARSER_EXPRVALUE_LVALUE) ||
               (rhs_valid && rhs->valtype == PARSER_EXPRVALUE_LVALUE) ||
               Parser_n_indir(&lhs->ret) > 0 || Parser_n_indir(&rhs->ret) > 0) {
        expr->valtype = PARSER_EXPRVALUE_LVALUE;
    } else {
        expr->valtype = PARSER_EXPRVALUE_XVALUE;
    }
}

static void typecheck_comma_expr(struct Parser_Expr *expr)
{
    auto rhs = &expr->info.args.arr[1];

    expr->valtype = rhs->valtype;
    expr->ret = Parser_copy_type(&rhs->ret);
}

static struct Diag cond_one_result_void_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = strdup("only one result of conditional '?' expression is void"),
        .err = ERRORTYPE_BAD_CONDITIONAL,
        .type = DIAGTYPE_ERROR,
    };
}

static void typecheck_conditional_expr(struct Parser_Expr *expr,
                                       struct DiagVec *diags)
{
    // e1 ? e2 : e3

    (void)cond_one_result_void_err(expr->tok);
    (void)expr;
    (void)diags;
    CRASH("haven't implemented typechecking the conditional operator");

#if 0
    auto e2 = &expr->info.args.arr[1];
    auto e3 = &expr->info.args.arr[2];

    bool e2_void =
        e2->ret.spec == PARSER_TYPESPEC_VOID && Parser_n_indir(&e2->ret) == 0;
    bool e3_void =
        e3->ret.spec == PARSER_TYPESPEC_VOID && Parser_n_indir(&e3->ret) == 0;

    if (e2_void && e3_void) {
        expr->valtype = PARSER_EXPRVALUE_PRVALUE;
        expr->ret = Parser_copy_type(&e2->ret);
    } else if (e2_void) {
        if (e3->type != PARSER_EXPRTYPE_THROW)
            gen_dynpush(diags, cond_one_result_void_err(e3->tok));
        expr->valtype = e3->valtype;
        expr->ret = Parser_copy_type(&e3->ret);
    } else if (e3_void) {
        if (e2->type != PARSER_EXPRTYPE_THROW)
            gen_dynpush(diags, cond_one_result_void_err(e2->tok));
        expr->valtype = e2->valtype;
        expr->ret = Parser_copy_type(&e2->ret);
    }
#endif
}

static struct Diag bad_operands(const struct Parser_Expr *expr,
                                const char *type, enum ErrorType err_type)
{
    bool unary = expr->info.args.len == 1;

    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    char *lhs_tname = Parser_type_to_str(&lhs->ret);
    char *rhs_tname = unary ? NULL : Parser_type_to_str(&rhs->ret);
    struct Diag ret;
    if (unary) {
        ret = (struct Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg = Print_fmt_to_str("%s operator can not operate on '%s'", type,
                                    lhs_tname),
            .err = err_type,
            .type = DIAGTYPE_ERROR,
        };
    } else {
        ret = (struct Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg =
                Print_fmt_to_str("%s operator can not operate on '%s' and '%s'",
                                 type, lhs_tname, rhs_tname),
            .err = err_type,
            .type = DIAGTYPE_ERROR,
        };
    }
    free(lhs_tname);
    free(rhs_tname);

    return ret;
}

static enum Parser_TypeSpec op_prom_typespec(enum Parser_TypeSpec spec)
{
    if (Parser_is_integral_typespec(spec))
        return Parser_integral_prom(spec);
    else if (Parser_is_floating_typespec(spec))
        return spec;
    else
        CRASH("can't promote type spec");
}

static struct Parser_Type op_prom_type(struct Parser_Type *type)
{
    auto ret = Parser_copy_type(type);
    ret.spec = op_prom_typespec(ret.spec);
    return ret;
}

static bool is_ptr_arith_op(enum Parser_ExprType op)
{
    return op == PARSER_EXPRTYPE_ADD || op == PARSER_EXPRTYPE_SUB;
}

static void typecheck_arith_bin_op_expr(struct Parser_Expr *expr,
                                        struct DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_ptr = Parser_n_indir(&lhs->ret) > 0;
    bool rhs_ptr = Parser_n_indir(&rhs->ret) > 0;

    bool bad_op_types;
    if (is_ptr_arith_op(expr->type)) {
        if (lhs_ptr && rhs_ptr)
            bad_op_types = true;
        else if (lhs_ptr)
            bad_op_types = !Parser_is_integral_typespec(rhs->ret.spec);
        else if (rhs_ptr)
            bad_op_types = Parser_is_integral_typespec(lhs->ret.spec);
        else
            bad_op_types = false;
    } else {
        bad_op_types = lhs_ptr || rhs_ptr;
    }

    if (bad_op_types) {
        gen_dynpush(diags, bad_operands(expr, "arithmetic",
                                        ERRORTYPE_BAD_ARITHMETIC_OP));
        expr->ret = Parser_copy_type(&lhs->ret);
    } else if (lhs_ptr) {
        expr->ret = Parser_copy_type(&lhs->ret);
    } else if (rhs_ptr) {
        expr->ret = Parser_copy_type(&rhs->ret);
    } else {
        i32 lhs_rank =
            Parser_typespec_conv_rank(op_prom_typespec(lhs->ret.spec));
        i32 rhs_rank =
            Parser_typespec_conv_rank(op_prom_typespec(rhs->ret.spec));
        expr->ret = lhs_rank > rhs_rank ? op_prom_type(&lhs->ret)
                                        : op_prom_type(&rhs->ret);
    }
}

static void typecheck_arith_unary_op_expr(struct Parser_Expr *expr,
                                          struct DiagVec *diags)
{
    auto arg = &expr->info.args.arr[0];
    expr->ret = op_prom_type(&arg->ret);

    bool arg_ptr = Parser_n_indir(&arg->ret) > 0;

    bool bad_op_types = arg_ptr;

    if (bad_op_types) {
        gen_dynpush(diags, bad_operands(expr, "arithmetic",
                                        ERRORTYPE_BAD_ARITHMETIC_OP));
    }
}

static void typecheck_arith_op_expr(struct Parser_Expr *expr,
                                    struct DiagVec *diags)
{
    expr->valtype = PARSER_EXPRVALUE_PRVALUE;

    if (Parser_is_unaryop(expr->type))
        typecheck_arith_unary_op_expr(expr, diags);
    else
        typecheck_arith_bin_op_expr(expr, diags);
}

// TODO: implement these
static void typecheck_logical_unary_op_expr(struct Parser_Expr *expr,
                                            struct DiagVec *diags)
{
    (void)expr;
    (void)diags;
}

static void typecheck_logical_bin_op_expr(struct Parser_Expr *expr,
                                          struct DiagVec *diags)
{
    (void)expr;
    (void)diags;
}

static void typecheck_logical_op_expr(struct Parser_Expr *expr,
                                      struct DiagVec *diags)
{
    expr->valtype = PARSER_EXPRVALUE_PRVALUE;
    expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_BOOL);

    if (Parser_is_unaryop(expr->type))
        typecheck_logical_unary_op_expr(expr, diags);
    else
        typecheck_logical_bin_op_expr(expr, diags);
}

static void typecheck_comp_op_expr(struct Parser_Expr *expr,
                                   struct DiagVec *diags)
{
    expr->valtype = PARSER_EXPRVALUE_PRVALUE;
    expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_BOOL);

    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    bool lhs_ptr = Parser_n_indir(&lhs->ret) > 0;
    bool lhs_void_ptr =
        lhs->ret.spec == PARSER_TYPESPEC_VOID && Parser_n_indir(&lhs->ret);
    bool rhs_ptr = Parser_n_indir(&rhs->ret) > 0;
    bool rhs_void_ptr =
        rhs->ret.spec == PARSER_TYPESPEC_VOID && Parser_n_indir(&rhs->ret);

    bool eq = lhs->ret.spec == rhs->ret.spec && Parser_n_indir(&lhs->ret) &&
              Parser_n_indir(&rhs->ret);

    // an arithmetic operator can operate on primitives, ptrs of the same type,
    // or a ptr and a void ptr
    bool bad_op_types =
        (lhs_ptr || rhs_ptr) && (!eq && (!lhs_void_ptr && !rhs_void_ptr));

    if (bad_op_types) {
        gen_dynpush(diags,
                    bad_operands(expr, "comp", ERRORTYPE_BAD_COMPARISON_OP));
    }
}

static struct Sema_Scope *bin_scope_res_scope(struct Parser_Expr *expr,
                                              struct Sema_Scope *scope)
{
    auto lhs = &expr->info.args.arr[0];
    assert(lhs->type == PARSER_EXPRTYPE_IDENTIFIER);

    struct Sema_Scope *base = Sema_closest_rnce_scope(scope);

    const char *name = lhs->info.ident;
    struct Sema_Scope *res = Sema_resolve_scope(name, base);

    assert(res);
    return res;
}

static struct Sema_Scope *unary_scope_res_scope(struct Sema_Scope *scope)
{
    auto res = scope;
    while (res->parent)
        res = res->parent;
    return res;
}

static void typecheck_scope_res_expr(struct Parser_Expr *expr,
                                     struct Sema_Scope *scope,
                                     struct DiagVec *diags)
{
    struct Sema_Scope *res;
    struct Parser_Expr *arg;
    if (expr->type == PARSER_EXPRTYPE_BIN_SCOPE_RES) {
        res = bin_scope_res_scope(expr, scope);
        arg = &expr->info.args.arr[1];
    } else {
        res = unary_scope_res_scope(scope);
        arg = &expr->info.args.arr[0];
    }

    Sema_typecheck_expr(arg, res, diags);

    expr->ret = Parser_copy_type(&arg->ret);
    expr->valtype = arg->valtype;
    expr->res_scope = Parser_is_scope_res(arg->type) ? arg->res_scope : res;
}

static struct Diag
memb_sel_lhs_not_class_err(const struct Parser_Expr *memb_sel)
{
    char *lhs_type = Parser_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct Diag ret = {
        .pos = memb_sel->tok->pos,
        .line = memb_sel->tok->line,
        .msg = Print_fmt_to_str(
            "member select lhs '%s' is not a class or union", lhs_type),
        .err = ERRORTYPE_BAD_MEMB_SEL,
        .type = DIAGTYPE_ERROR};

    free(lhs_type);
    return ret;
}

static struct Diag memb_sel_expects_ptr_err(const struct Parser_Expr *memb_sel)
{
    char *lhs_type = Parser_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct Diag ret = {.pos = memb_sel->tok->pos,
                       .line = memb_sel->tok->line,
                       .msg = Print_fmt_to_str(
                           "member select lhs '%s' is not a pointer", lhs_type),
                       .err = ERRORTYPE_BAD_MEMB_SEL,
                       .type = DIAGTYPE_ERROR};

    free(lhs_type);
    return ret;
}

static struct Diag
memb_sel_expects_non_ptr_err(const struct Parser_Expr *memb_sel)
{
    char *lhs_type = Parser_type_to_str(&memb_sel->info.args.arr[0].ret);

    struct Diag ret = {.pos = memb_sel->tok->pos,
                       .line = memb_sel->tok->line,
                       .msg = Print_fmt_to_str(
                           "member select lhs '%s' is a pointer", lhs_type),
                       .err = ERRORTYPE_BAD_MEMB_SEL,
                       .type = DIAGTYPE_ERROR};

    free(lhs_type);
    return ret;
}

static bool memb_sel_expects_ptr(enum Parser_ExprType type)
{
    return type == PARSER_EXPRTYPE_PTR_MEMB_SEL ||
           type == PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;
}

static struct Diag unknown_field_err(const char *field, const char *class_,
                                     bool is_union,
                                     const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("unknown field '%s' in %s '%s'", field,
                                is_union ? "union" : "class", class_),
        .err = ERRORTYPE_BAD_IDENTIFIER,
        .type = DIAGTYPE_ERROR,
    };
}

static void typecheck_memb_sel(struct Parser_Expr *expr,
                               struct Sema_Scope *scope, struct DiagVec *diags)
{
    auto lhs = &expr->info.args.arr[0];
    auto rhs = &expr->info.args.arr[1];

    Sema_typecheck_expr(lhs, scope, diags);

    if (lhs->ret.spec != PARSER_TYPESPEC_CLASS &&
        lhs->ret.spec != PARSER_TYPESPEC_UNION) {
        gen_dynpush(diags, memb_sel_lhs_not_class_err(expr));
        return;
    } else if (memb_sel_expects_ptr(expr->type) &&
               Parser_n_indir(&lhs->ret) != 1) {
        gen_dynpush(diags, memb_sel_expects_ptr_err(expr));
    } else if (!memb_sel_expects_ptr(expr->type) &&
               Parser_n_indir(&lhs->ret) != 0) {
        gen_dynpush(diags, memb_sel_expects_non_ptr_err(expr));
    } else if (rhs->type != PARSER_EXPRTYPE_IDENTIFIER) {
        gen_dynpush(diags,
                    Diag_expected_token_err("identifier", expr->tok,
                                            ERRORTYPE_MISSING_IDENTIFIER));
        return;
    }

    const struct Parser_Class *class_ =
        &Parser_named_type_ident(&lhs->ret.named)->decl->class_;
    const char *field_name = rhs->info.ident;
    isize_t field_idx = Parser_find_field(class_, field_name);
    if (field_idx == -1) {
        gen_dynpush(diags,
                    unknown_field_err(field_name, class_->name,
                                      class_->type == PARSER_CLASSTYPE_UNION,
                                      rhs->tok));
        return;
    }

    auto field = class_->childs.arr[field_idx];

    if (field->type == PARSER_ASTNODETYPE_VAR_DECL) {
        expr->ret = Parser_copy_type(
            &Parser_decl_inst_of_name(&field->var_decl, field_name)->type);
        expr->valtype = PARSER_EXPRVALUE_LVALUE;
    } else {
        expr->ret = Parser_create_func_type(
            Parser_class_ident(class_)->class_info.def_scope,
            field->func_decl.name);
        expr->valtype = PARSER_EXPRVALUE_LVALUE;
    }
}

static void typecheck_op_expr(struct Parser_Expr *expr,
                              struct Sema_Scope *scope, struct DiagVec *diags)
{
    if (expr->type == PARSER_EXPRTYPE_FUNC_CALL)
        typecheck_call_expr(expr, scope, diags);
    else if (Parser_is_assignment(expr->type))
        typecheck_assignment_expr(expr, diags);
    else if (expr->type == PARSER_EXPRTYPE_PREFIX_INC ||
             expr->type == PARSER_EXPRTYPE_PREFIX_DEC ||
             expr->type == PARSER_EXPRTYPE_POSTFIX_INC ||
             expr->type == PARSER_EXPRTYPE_POSTFIX_DEC)
        typecheck_inc_dec_expr(expr, diags);
    else if (expr->type == PARSER_EXPRTYPE_DEREF)
        typecheck_deref_expr(expr, diags);
    else if (expr->type == PARSER_EXPRTYPE_REF)
        typecheck_ref_expr(expr, diags);
    else if (expr->type == PARSER_EXPRTYPE_ARRAY_SUBSCR)
        typecheck_arr_subscr_expr(expr, diags);
    else if (expr->type == PARSER_EXPRTYPE_COMMA)
        typecheck_comma_expr(expr);
    else if (expr->type == PARSER_EXPRTYPE_CONDITIONAL)
        typecheck_conditional_expr(expr, diags);
    else if (Parser_is_arith_op(expr->type))
        typecheck_arith_op_expr(expr, diags);
    else if (Parser_is_logical_op(expr->type))
        typecheck_logical_op_expr(expr, diags);
    else if (Parser_is_comp_op(expr->type))
        typecheck_comp_op_expr(expr, diags);
    else if (Parser_is_scope_res(expr->type))
        typecheck_scope_res_expr(expr, scope, diags);
    else if (Parser_is_memb_sel(expr->type))
        typecheck_memb_sel(expr, scope, diags);
    else {
        printf("op at %d:%d\n", expr->tok->pos.line, expr->tok->pos.column);
        printf("op type = %d\n", expr->type);
        CRASH("typechecking op not implemented");
    }
}

static void typecheck_overloaded_op(struct Parser_Expr *expr,
                                    struct Parser_ASTNode *overload)
{
    printf("found op overload at %d:%d\n", expr->tok->pos.line,
           expr->tok->pos.column);
    const struct Parser_FuncDecl *func = &overload->func_decl;
    printf("op overload decl at %d:%d\n", overload->start->pos.line,
           overload->start->pos.column);

    expr->overloaded = true;
    expr->node = overload;
    expr->ret = Parser_copy_type(&func->type);

    if (func->type.lv_ref)
        expr->valtype = PARSER_EXPRVALUE_LVALUE;
    else if (func->type.rv_ref)
        expr->valtype = PARSER_EXPRVALUE_XVALUE;
    else
        expr->valtype = PARSER_EXPRVALUE_PRVALUE;
}

void Sema_typecheck_expr(struct Parser_Expr *expr, struct Sema_Scope *scope,
                         struct DiagVec *diags)
{
    if (expr->typechecked)
        return;
    expr->typechecked = true;

    if (Parser_is_numlit(expr->type)) {
        typecheck_lit_expr(expr);
    } else if (expr->type == PARSER_EXPRTYPE_IDENTIFIER) {
        typecheck_ident_expr(expr, scope, diags);
    } else if (expr->type == PARSER_EXPRTYPE_THIS) {
        typecheck_this_expr(expr, scope, diags);
    } else {
        // some operators are weird
        bool typecheck_args =
            !Parser_is_scope_res(expr->type) && !Parser_is_memb_sel(expr->type);
        if (typecheck_args) {
            for (isize_t i = 0; i < expr->info.args.len; ++i)
                Sema_typecheck_expr(&expr->info.args.arr[i], scope, diags);
        }

        struct Parser_ASTNode *overload = Sema_find_op_overload(
            expr->type, expr->info.args.arr, expr->info.args.len, scope);
        if (!overload)
            typecheck_op_expr(expr, scope, diags);
        else
            typecheck_overloaded_op(expr, overload);
    }
}

static struct Diag no_matching_ctor_err(const struct Parser_Type *type,
                                        const struct Lexer_Token *tok)
{
    char *str = Parser_type_to_str(type);

    struct Diag ret = {
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("no matching constructor for '%s'", str),
        .err = ERRORTYPE_NO_MATCHING_CTOR,
        .type = DIAGTYPE_ERROR,
    };

    free(str);
    return ret;
}

// returns whether or not the ctor is correct
static bool typecheck_vdecl_class_type_ctor(struct Parser_VarDeclInst *inst)
{
    assert(inst->has_ctor);

    auto ident = Parser_named_type_ident(&inst->type.named);
    inst->ctor.node =
        Sema_find_func(ident->name, inst->ctor.args.arr, inst->ctor.args.len,
                       ident->class_info.def_scope, true);

    return inst->ctor.node != NULL;
}

// returns whether or not the ctor is correct
static bool typecheck_vdecl_generic_type_ctor(struct Parser_VarDeclInst *inst)
{
    assert(inst->has_ctor);

    if (inst->ctor.args.len > 1)
        return false;

    auto arg = &inst->ctor.args.arr[0];
    return Sema_can_convert(&arg->ret, arg->valtype, &inst->type);
}

void Sema_typecheck_var_decl_inst(struct Parser_VarDeclInst *inst,
                                  struct DiagVec *diags)
{
    inst->typechecked = true;
    if (!inst->has_ctor)
        return;

    bool bad;
    if ((inst->type.spec == PARSER_TYPESPEC_CLASS ||
         inst->type.spec == PARSER_TYPESPEC_UNION) &&
        Parser_n_indir(&inst->type) == 0) {

        bad = !typecheck_vdecl_class_type_ctor(inst);
    } else {
        bad = !typecheck_vdecl_generic_type_ctor(inst);
    }

    if (bad)
        gen_dynpush(diags, no_matching_ctor_err(&inst->type, inst->start));
}

static struct Diag
invalid_return_stmt_type_err(const struct Parser_Type *func_type,
                             const struct Parser_Type *ret_type,
                             const struct Lexer_Token *tok)
{
    char *func_type_str = Parser_type_to_str(func_type);

    struct Diag ret;

    if (ret_type) {
        char *ret_type_str = Parser_type_to_str(ret_type);

        ret = (struct Diag){
            .pos = tok->pos,
            .line = tok->line,
            .msg = Print_fmt_to_str("returning '%s' in function of type '%s'",
                                    ret_type_str, func_type_str),
            .err = ERRORTYPE_BAD_RETURN_STMT_TYPE,
            .type = DIAGTYPE_ERROR,
        };

        free(ret_type_str);
    } else {
        ret = (struct Diag){
            .pos = tok->pos,
            .line = tok->line,
            .msg = Print_fmt_to_str(
                "expected a return value in function of type '%s'",
                func_type_str),
            .err = ERRORTYPE_BAD_RETURN_STMT_TYPE,
            .type = DIAGTYPE_ERROR,
        };
    }

    free(func_type_str);

    return ret;
}

static struct Diag return_outside_func_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("return statement outside a function"),
        .err = ERRORTYPE_RETURN_OUTSIDE_FUNC,
        .type = DIAGTYPE_ERROR,
    };
}

void Sema_typecheck_return(const struct Parser_ASTNode *node,
                           const struct Sema_Scope *scope,
                           struct DiagVec *diags)
{
    auto func_scope =
        Sema_closest_scope_of_type_const(scope, SEMA_SCOPETYPE_FUNC);
    if (!func_scope) {
        gen_dynpush(diags, return_outside_func_err(node->start));
        return;
    }

    const struct Parser_Type *func_type = &func_scope->node->func_decl.type;

    bool is_void = func_type->spec == PARSER_TYPESPEC_VOID &&
                   Parser_n_indir(func_type) == 0;

    if (node->ret.expr) {
        if (!Sema_can_convert(&node->ret.expr->ret, node->ret.expr->valtype,
                              func_type))
            gen_dynpush(diags,
                        invalid_return_stmt_type_err(
                            func_type, &node->ret.expr->ret, node->start));
    } else if (!is_void) {
        gen_dynpush(diags,
                    invalid_return_stmt_type_err(func_type, NULL, node->start));
    }
}

static bool is_valid_array_to_ptr(const struct Parser_Type *src,
                                  const struct Parser_Type *dest)
{
    if (src->spec != PARSER_TYPESPEC_ARRAY)
        return false;

    // an array to ptr conversion becomes a prvalue so it can't be passed to a
    // non-const lvalue reference
    if (dest->lv_ref && !dest->dquals.arr[0].is_const)
        return false;

    isize_t src_indir = Parser_n_indir(src);
    isize_t elem_indir = Parser_n_indir(&src->array->elem);
    isize_t dest_indir = Parser_n_indir(dest);

    if (src_indir != 0 || elem_indir + 1 != dest_indir)
        return false;
    else if (src->array->elem.spec != dest->spec)
        return false;
    else if (!Parser_dquals_same(src->array->elem.dquals.arr,
                                 src->array->elem.dquals.len,
                                 &dest->dquals.arr[1], dest->dquals.len - 1))
        return false;

    return true;
}

bool Sema_can_convert(const struct Parser_Type *src,
                      enum Parser_ExprValueType src_valtype,
                      const struct Parser_Type *dest)
{
    // rv references cannot take lvalues and non-const lv rereferences
    // cannot take rvalues
    if ((dest->rv_ref && !Parser_is_rvalue(src_valtype)) ||
        (dest->lv_ref && !dest->dquals.arr[0].is_const &&
         Parser_is_rvalue(src_valtype)))
        return false;

    if (Parser_is_fundamental_type(src) && Parser_is_fundamental_type(dest))
        return true;
    else if (Parser_n_indir(src) == Parser_n_indir(dest) &&
             src->spec == dest->spec)
        return true;
    else if (Parser_n_indir(src) > 0 && Parser_type_is_void_ptr(dest))
        return true;
    else if (Parser_type_is_nullptr_t(src) && Parser_n_indir(dest) > 0)
        return true;
    else if (is_valid_array_to_ptr(src, dest))
        return true;

    return false;
}

int Sema_conversion_rank(const struct Parser_Type *src,
                         const struct Parser_Type *dest)
{
    bool same_indir = Parser_n_indir(src) == Parser_n_indir(dest);
    bool no_indir = Parser_n_indir(src) == 0 && Parser_n_indir(dest) == 0;

    bool src_int = Parser_is_integral_typespec(src->spec);
    bool dest_int = Parser_is_integral_typespec(dest->spec);
    bool src_flt = Parser_is_floating_typespec(src->spec);
    bool dest_flt = Parser_is_floating_typespec(dest->spec);

    if (same_indir && src->spec == dest->spec)
        return 1;
    else if (no_indir && ((src_int && dest_int) || (src_flt && dest_flt)))
        return 2;
    else
        return 3;
}
