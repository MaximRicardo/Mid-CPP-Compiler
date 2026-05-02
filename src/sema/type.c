#include "type.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ident.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool Sema_is_typespec(const struct Lexer_Token *tok,
                      const struct Parser_ASTNode *parent)
{
    if (Lexer_is_typespec(tok->type))
        return true;
    else if (tok->type == LEXER_TOKENTYPE_IDENTIFIER)
        return Sema_find_type_const(tok->ident, parent, tok);
    else
        return false;
}

struct Parser_Type Sema_typespec_type(const struct Lexer_Token *tok,
                                      const struct Parser_ASTNode *parent)
{

    if (tok->type == LEXER_TOKENTYPE_STRUCT ||
        tok->type == LEXER_TOKENTYPE_CLASS ||
        tok->type == LEXER_TOKENTYPE_ENUM ||
        tok->type == LEXER_TOKENTYPE_UNION) {
        CRASH("can't convert composed types to a type spec");
    } else if (tok->type == LEXER_TOKENTYPE_IDENTIFIER) {
        auto node = Sema_find_type_const(tok->ident, parent, tok);
        switch (node->type) {
        case PARSER_ASTNODETYPE_CLASS:
            return Parser_toktype_to_type(node->class_.is_union
                                              ? LEXER_TOKENTYPE_UNION
                                              : LEXER_TOKENTYPE_CLASS,
                                          node->class_.name);

        case PARSER_ASTNODETYPE_ENUM:
            return Parser_toktype_to_type(LEXER_TOKENTYPE_ENUM,
                                          node->enum_.name);

        case PARSER_ASTNODETYPE_VAR_DECL:
            // falls through to default if false
            if (node->var_decl.type.squals.is_typedef)
                return Parser_copy_type(&node->var_decl.type);
        default:
            CRASH("node doesn't hold a type");
        }
    } else {
        return Parser_toktype_to_type(tok->type, NULL);
    }
}

bool Sema_node_is_type(const struct Parser_ASTNode *node)
{
    return node->type == PARSER_ASTNODETYPE_ENUM ||
           node->type == PARSER_ASTNODETYPE_CLASS ||
           (node->type == PARSER_ASTNODETYPE_VAR_DECL &&
            node->var_decl.type.squals.is_typedef);
}

const char *Sema_node_type_name(const struct Parser_ASTNode *node)
{
    if (node->type == PARSER_ASTNODETYPE_ENUM)
        return node->enum_.name;
    else if (node->type == PARSER_ASTNODETYPE_CLASS)
        return node->class_.name;
    else if (node->type == PARSER_ASTNODETYPE_VAR_DECL &&
             node->var_decl.type.squals.is_typedef)
        return node->var_decl.name;
    else
        CRASH("ast node doesn't have a type name");
}

const struct Parser_ASTNode *
Sema_find_type_const(const char *name, const struct Parser_ASTNode *node,
                     const struct Lexer_Token *end)
{
    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (subs->arr[i]->start >= end)
                break;

            if (Sema_node_is_type(subs->arr[i]) &&
                !strcmp(Sema_node_type_name(subs->arr[i]), name))
                return subs->arr[i];
        }
    }

    if (node->parent)
        return Sema_find_type_const(name, node->parent, end);
    else
        return NULL;
}

struct Parser_ASTNode *Sema_find_type(const char *name,
                                      struct Parser_ASTNode *node,
                                      const struct Lexer_Token *end)
{
    return (struct Parser_ASTNode *)Sema_find_type_const(name, node, end);
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

    case PARSER_EXPRTYPE_PTR_LIT:
        expr->ret.spec = PARSER_TYPESPEC_VOID;
        gen_dynpush(&expr->ret.dquals, (struct Parser_TypeDataQual){});
        break;

    default:
        CRASH("expr isn't a literal");
    }
}

static void typecheck_ident_expr(struct Parser_Expr *expr,
                                 const struct Parser_ASTNode *parent,
                                 struct DiagVec *diags)
{
    assert(expr->type == PARSER_EXPRTYPE_IDENTIFIER);

    // all identifiers are lvalues
    expr->valtype = PARSER_EXPRVALUE_LVALUE;

    const struct Parser_Type *type =
        Sema_ident_type_const(expr->tok->ident, parent);

    if (!type) {
        gen_dynpush(diags,
                    ((struct Diag){
                        .pos = expr->tok->pos,
                        .line = expr->tok->line,
                        .msg = Print_fmt_to_str("undeclared identifier '%s'",
                                                expr->tok->ident),
                        .err = ERRORTYPE_UNDECLARED_IDENTIFIER,
                        .is_err = true,
                    }));
        expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_INT, NULL);
    } else {
        expr->ret = Parser_copy_type(type);
    }
}

static void set_func_call_node(struct Parser_Expr *expr,
                               struct Parser_ASTNode *parent,
                               struct DiagVec *diags)
{
    const struct Parser_Expr *name_expr = &expr->info.args.arr[0];

    if (name_expr->type == PARSER_EXPRTYPE_IDENTIFIER) {
        expr->node = Sema_ident_creation(name_expr->info.ident, parent);
        if (!expr->node || expr->node->type != PARSER_ASTNODETYPE_FUNC_DECL)
            gen_dynpush(diags,
                        ((struct Diag){
                            .pos = name_expr->tok->pos,
                            .line = name_expr->tok->line,
                            .msg = Print_fmt_to_str("'%s' is not a function",
                                                    name_expr->info.ident),
                            .err = ERRORTYPE_BAD_IDENTIFIER,
                            .is_err = true,
                        }));
    } else
        CRASH("calling function ptrs not implemented");
}

static void typecheck_call_expr(struct Parser_Expr *expr,
                                struct Parser_ASTNode *parent,
                                struct DiagVec *diags)
{
    set_func_call_node(expr, parent, diags);
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
                .is_err = true,
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
                .is_err = true,
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
                .is_err = true,
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
                        .is_err = true,
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
                .is_err = true,
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
        .is_err = true,
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
            .is_err = true,
        };
    } else {
        ret = (struct Diag){
            .pos = expr->tok->pos,
            .line = expr->tok->line,
            .msg =
                Print_fmt_to_str("%s operator can not operate on '%s' and '%s'",
                                 type, lhs_tname, rhs_tname),
            .err = err_type,
            .is_err = true,
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
    expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_BOOL, NULL);

    if (Parser_is_unaryop(expr->type))
        typecheck_logical_unary_op_expr(expr, diags);
    else
        typecheck_logical_bin_op_expr(expr, diags);
}

static void typecheck_comp_op_expr(struct Parser_Expr *expr,
                                   struct DiagVec *diags)
{
    expr->valtype = PARSER_EXPRVALUE_PRVALUE;
    expr->ret = Parser_toktype_to_type(LEXER_TOKENTYPE_BOOL, NULL);

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

static void typecheck_op_expr(struct Parser_Expr *expr,
                              struct Parser_ASTNode *parent,
                              struct DiagVec *diags)
{
    if (expr->type == PARSER_EXPRTYPE_FUNC_CALL)
        typecheck_call_expr(expr, parent, diags);
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
    else {
        printf("op at %d:%d\n", expr->tok->pos.line, expr->tok->pos.column);
        printf("op type = %d\n", expr->type);
        CRASH("typechecking op not implemented");
    }
}

static isize_t best_op_overload(const struct Parser_ASTNodePVec *overloads,
                                struct Parser_Expr *expr)
{
    for (isize_t i = 0; i < overloads->len; ++i) {
        const struct Parser_FuncDecl *func = &overloads->arr[i]->func_decl;
        if (func->params.len != expr->info.args.len)
            continue;

        bool bad = false;
        for (isize_t j = 0; j < func->params.len; ++j) {
            auto param = &func->params.arr[j];
            auto arg = &expr->info.args.arr[j];

            isize_t param_indir = Parser_n_indir(&param->type);
            isize_t arg_indir = Parser_n_indir(&arg->ret);

            if (param->type.spec != arg->ret.spec || param_indir != arg_indir) {
                bad = true;
                break;
            }

            // rv references cannot take lvalues and non-const lv rereferences
            // cannot take rvalues
            if ((param->type.rv_ref && !Parser_is_rvalue(arg->valtype)) ||
                (param->type.lv_ref && !param->type.dquals.arr[0].is_const &&
                 Parser_is_rvalue(arg->valtype))) {
                bad = true;
                break;
            }
        }

        if (!bad)
            return i;
    }

    return -1;
}

static void typecheck_overloaded_op(struct Parser_Expr *expr,
                                    struct Parser_ASTNode *parent,
                                    const struct Parser_ASTNodePVec *overloads,
                                    struct DiagVec *diags)
{
    isize_t best = best_op_overload(overloads, expr);
    if (best == -1)
        typecheck_op_expr(expr, parent, diags);
    else {
        printf("found overload at %d:%d\n", expr->tok->pos.line,
               expr->tok->pos.column);
        const struct Parser_FuncDecl *func = &overloads->arr[best]->func_decl;
        printf("overload decl at %d:%d\n",
               overloads->arr[best]->start->pos.line,
               overloads->arr[best]->start->pos.column);

        expr->ret = Parser_copy_type(&func->type);

        if (func->type.lv_ref)
            expr->valtype = PARSER_EXPRVALUE_LVALUE;
        else if (func->type.rv_ref)
            expr->valtype = PARSER_EXPRVALUE_XVALUE;
        else
            expr->valtype = PARSER_EXPRVALUE_PRVALUE;
    }
}

void Sema_typecheck_expr(struct Parser_Expr *expr,
                         struct Parser_ASTNode *parent, struct DiagVec *diags)
{
    if (Parser_is_numlit(expr->type)) {
        typecheck_lit_expr(expr);
    } else if (expr->type == PARSER_EXPRTYPE_IDENTIFIER) {
        typecheck_ident_expr(expr, parent, diags);
    } else {
        for (isize_t i = 0; i < expr->info.args.len; ++i) {
            Sema_typecheck_expr(&expr->info.args.arr[i], parent, diags);
        }

        auto overloads = Sema_op_overloads(expr->type, parent, expr->tok);
        if (overloads.len == 0)
            typecheck_op_expr(expr, parent, diags);
        else
            typecheck_overloaded_op(expr, parent, &overloads, diags);
        gen_dyndeinit(&overloads);
    }
}

void Sema_typecheck_root(struct Parser_ASTNode *node, struct DiagVec *diags)
{
    assert(node->type == PARSER_ASTNODETYPE_ROOT);

    for (isize_t i = 0; i < node->root.len; ++i)
        Sema_typecheck_node(node->root.arr[i], diags);
}

void Sema_typecheck_var_decl(struct Parser_VarDecl *decl,
                             struct Parser_ASTNode *node, struct DiagVec *diags)
{
    if (decl->init)
        Sema_typecheck_expr(decl->init, node, diags);
}

void Sema_typecheck_func_decl(struct Parser_FuncDecl *decl,
                              struct Parser_ASTNode *node,
                              struct DiagVec *diags)
{
    for (isize_t i = 0; i < decl->params.len; ++i)
        Sema_typecheck_var_decl(&decl->params.arr[i], node, diags);

    for (isize_t i = 0; i < decl->nodes.len; ++i)
        Sema_typecheck_node(decl->nodes.arr[i], diags);
}

void Sema_typecheck_node(struct Parser_ASTNode *node, struct DiagVec *diags)
{
    switch (node->type) {
    case PARSER_ASTNODETYPE_ROOT:
        Sema_typecheck_root(node, diags);
        break;

    case PARSER_ASTNODETYPE_EXPR:
        Sema_typecheck_expr(&node->expr, node, diags);
        break;

    case PARSER_ASTNODETYPE_VAR_DECL:
        Sema_typecheck_var_decl(&node->var_decl, node, diags);
        break;

    case PARSER_ASTNODETYPE_FUNC_DECL:
        Sema_typecheck_func_decl(&node->func_decl, node, diags);
        break;

    default:
        CRASH("can't typecheck node");
    }
}
