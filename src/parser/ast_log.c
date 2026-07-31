#include "ast_log.h"
#include "attribute.h"
#include "ints.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

constexpr int indent_width = 4;

MID_ATTRIBUTE((format(printf, 3, 4)))
static void log_w_indent(FILE *out, int indent, const char *fmt, ...)
{
    for (int i = 0; i < indent; ++i) {
        for (int j = 0; j < indent_width; ++j)
            fputc(' ', out);
    }

    va_list args;
    va_start(args);

    vfprintf(out, fmt, args);

    va_end(args);
}

static void log_expr(const struct MidParser_Expr *expr, FILE *out);

static void log_lit_expr(const struct MidParser_Expr *expr, FILE *out)
{
    switch (expr->type) {
    case MIDPARSER_EXPRTYPE_CHAR_LIT:
    case MIDPARSER_EXPRTYPE_STRING_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        break;

    case MIDPARSER_EXPRTYPE_WCHAR_LIT:
    case MIDPARSER_EXPRTYPE_WSTRING_LIT:
        fputc('L', out);
        MidLit_fprint(out, expr->info.val, expr->type);
        break;

    case MIDPARSER_EXPRTYPE_CHAR16_LIT:
    case MIDPARSER_EXPRTYPE_STRING16_LIT:
        fputc('u', out);
        MidLit_fprint(out, expr->info.val, expr->type);
        break;

    case MIDPARSER_EXPRTYPE_CHAR32_LIT:
    case MIDPARSER_EXPRTYPE_STRING32_LIT:
        fputc('U', out);
        MidLit_fprint(out, expr->info.val, expr->type);
        break;

    case MIDPARSER_EXPRTYPE_UINT_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        fprintf(out, "u");
        break;

    case MIDPARSER_EXPRTYPE_LONG_LIT:
    case MIDPARSER_EXPRTYPE_LONGDOUBLE_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        fprintf(out, "l");
        break;

    case MIDPARSER_EXPRTYPE_ULONG_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        fprintf(out, "ul");
        break;

    case MIDPARSER_EXPRTYPE_LONGLONG_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        fprintf(out, "ll");
        break;

    case MIDPARSER_EXPRTYPE_ULONGLONG_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        fprintf(out, "ull");
        break;

    case MIDPARSER_EXPRTYPE_FLOAT_LIT:
        MidLit_fprint(out, expr->info.val, expr->type);
        fprintf(out, "f");
        break;

    default:
        MidLit_fprint(out, expr->info.val, expr->type);
        break;
    }
}

static void log_ident_expr(const struct MidParser_Expr *expr, FILE *out)
{
    assert(expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER);

    fprintf(out, "%s", expr->info.ident);
}

static void log_this_expr(const struct MidParser_Expr *expr, FILE *out)
{
    assert(expr->type == MIDPARSER_EXPRTYPE_THIS);

    fprintf(out, "this");
}

static void log_scope_res_expr(const struct MidParser_Expr *expr, FILE *out)
{
    if (expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER) {
        fprintf(out, "%s", expr->info.ident);
    } else {
        struct MidParser_Expr *scope = expr->type == MIDPARSER_EXPRTYPE_BIN_SCOPE_RES
                                        ? &expr->info.args.arr[0]
                                        : NULL;
        struct MidParser_Expr *child = expr->type == MIDPARSER_EXPRTYPE_BIN_SCOPE_RES
                                        ? &expr->info.args.arr[1]
                                        : &expr->info.args.arr[0];

        if (scope && scope->type == MIDPARSER_EXPRTYPE_IDENTIFIER)
            fprintf(out, "%s", scope->info.ident);
        fprintf(out, "::");

        log_scope_res_expr(child, out);
    }
}

// doesn't include parentheses
static void log_func_call_args(const struct MidParser_Expr *args, mid_isize n,
                               FILE *out)
{
    for (mid_isize i = 0; i < n; ++i) {
        log_expr(&args[i], out);

        if (i + 1 < n)
            fprintf(out, ", ");
    }
}

static void log_func_call_expr(const struct MidParser_Expr *expr, FILE *out)
{
    const struct MidParser_Expr *dest = &expr->info.args.arr[0];
    if (MidParser_is_scope_res(dest->type))
        log_scope_res_expr(dest, out);
    else if (MidParser_is_memb_sel(dest->type))
        log_expr(dest, out);
    else if (dest->ret.spec == MIDPARSER_TYPESPEC_FUNC)
        fprintf(out, "%s", dest->ret.func.name);
    else
        MID_CRASH("func call dest expr not supported");

    fprintf(out, "#%" PRIi32 ":%" PRIi32 "(", expr->node->start->pos.line,
            expr->node->start->pos.column);

    log_func_call_args(&expr->info.args.arr[1], expr->info.args.len - 1, out);

    fprintf(out, ")");
}

static void log_generic_expr(const struct MidParser_Expr *expr, FILE *out)
{
    fprintf(out, "_%s", MidParser_exprtype_name(expr->type));
    if (expr->overloaded)
        fprintf(out, "#%" PRIi32 ":%" PRIi32, expr->node->start->pos.line,
                expr->node->start->pos.column);

    fprintf(out, "(");

    for (mid_isize i = 0; i < expr->info.args.len; ++i) {
        log_expr(&expr->info.args.arr[i], out);

        if (i + 1 < expr->info.args.len)
            fprintf(out, ", ");
    }

    fprintf(out, ")");
}

static void log_expr(const struct MidParser_Expr *expr, FILE *out)
{
    if (expr->type == MIDPARSER_EXPRTYPE_IDENTIFIER)
        log_ident_expr(expr, out);
    else if (expr->type == MIDPARSER_EXPRTYPE_THIS)
        log_this_expr(expr, out);
    else if (MidParser_is_numlit(expr->type))
        log_lit_expr(expr, out);
    else if (MidParser_is_scope_res(expr->type))
        log_scope_res_expr(expr, out);
    else if (expr->type == MIDPARSER_EXPRTYPE_FUNC_CALL)
        log_func_call_expr(expr, out);
    else
        log_generic_expr(expr, out);
}

static void log_func_params(const struct MidParser_ASTNode *node, FILE *out)
{
    mid_isize end = node->func_decl.params.len;
    for (mid_isize i = 0; i < end; ++i) {
        auto param = node->func_decl.params.arr[i]->insts.arr[0];

        char *type = MidParser_type_to_str(&param->type);
        fprintf(out, "%s", type);
        free(type);
        type = NULL;

        if (param->name)
            fprintf(out, " %s", param->name);
        if (i + 1 < end)
            fprintf(out, ", ");
    }
}

static void log_tor_entry(const struct MidParser_ASTNode *node, FILE *out,
                          int indent, bool is_dtor)
{
    const char *type = node->func_decl.param_scope->parent->node->class_.name;
    if (is_dtor)
        log_w_indent(out, indent, "~%s", type);
    else
        log_w_indent(out, indent, "%s", type);
    type = NULL;

    fprintf(out, "(");
    log_func_params(node, out);
    fprintf(out, ")");
}

static void log_generic_func_entry(const struct MidParser_ASTNode *node, FILE *out,
                                   int indent)
{
    char *type = MidParser_type_to_str(&node->func_decl.ret);
    log_w_indent(out, indent, "%s %s", type, node->func_decl.name);
    free(type);
    type = NULL;

    if (node->func_decl.is_op_overload)
        fprintf(out, "#%s", MidParser_exprtype_name(node->func_decl.op_overload));

    fprintf(out, "(");
    log_func_params(node, out);
    fprintf(out, ")");
}

static void log_func_entry(const struct MidParser_ASTNode *node, FILE *out,
                           int indent)
{
    if (node->func_decl.is_tor)
        log_tor_entry(node, out, indent, node->func_decl.is_dtor);
    else
        log_generic_func_entry(node, out, indent);
}

static void log_func_node(const struct MidParser_ASTNode *node, FILE *out,
                          int indent)
{
    log_func_entry(node, out, indent);
    if (node->func_decl.nodes.len == 0) {
        fprintf(out, ";\n\n");
        return;
    }

    fputc('\n', out);
    log_w_indent(out, indent, "{\n");

    for (mid_isize i = 0; i < node->func_decl.nodes.len; ++i) {
        const struct MidParser_ASTNode *child = node->func_decl.nodes.arr[i];
        MidParser_log_node(child, out, indent + 1);
    }

    log_w_indent(out, indent, "}\n\n");
}

static void log_class_supers(const struct MidParser_ASTNode *node, FILE *out)
{
    if (node->class_.supers.len > 0) {
        fprintf(out, " : ");
        return;
    }

    mid_isize end = node->class_.supers.len;
    for (mid_isize i = 0; i < end; ++i) {
        fprintf(out, "%s", node->class_.supers.arr[i]->name);

        if (i + 1 < end)
            fprintf(out, ", ");
    }
}

static void log_class_entry(const struct MidParser_ASTNode *node, FILE *out,
                            int indent)
{
    switch (node->class_.type) {
    case MIDPARSER_CLASSTYPE_CLASS:
        log_w_indent(out, indent, "class ");
        break;

    case MIDPARSER_CLASSTYPE_STRUCT:
        log_w_indent(out, indent, "struct ");
        break;

    case MIDPARSER_CLASSTYPE_UNION:
        log_w_indent(out, indent, "union ");
        break;
    }

    fprintf(out, "%s", node->class_.name);
    log_class_supers(node, out);
}

static void log_class_node(const struct MidParser_ASTNode *node, FILE *out,
                           int indent)
{
    log_class_entry(node, out, indent);

    bool empty = node->class_.pub_childs.len == 0 &&
                 node->class_.priv_childs.len == 0 &&
                 node->class_.prot_childs.len == 0;
    if (empty) {
        fprintf(out, ";\n\n");
        return;
    }

    fputc('\n', out);
    log_w_indent(out, indent, "{\n");

    log_w_indent(out, indent, "public:\n");
    for (mid_isize i = 0; i < node->class_.pub_childs.len; ++i) {
        const struct MidParser_ASTNode *child = node->class_.pub_childs.arr[i];
        MidParser_log_node(child, out, indent + 1);
    }

    log_w_indent(out, indent, "private:\n");
    for (mid_isize i = 0; i < node->class_.priv_childs.len; ++i) {
        const struct MidParser_ASTNode *child = node->class_.priv_childs.arr[i];
        MidParser_log_node(child, out, indent + 1);
    }

    log_w_indent(out, indent, "protected:\n");
    for (mid_isize i = 0; i < node->class_.prot_childs.len; ++i) {
        const struct MidParser_ASTNode *child = node->class_.prot_childs.arr[i];
        MidParser_log_node(child, out, indent + 1);
    }

    log_w_indent(out, indent, "};\n");

    if (node->class_.var)
        MidParser_log_node(MIDPARSER_GET_NODE(node->class_.var), out, indent);
    else
        fprintf(out, "\n");
}

static void log_namespace_node(const struct MidParser_ASTNode *node, FILE *out,
                               int indent)
{
    log_w_indent(out, indent, "namespace %s\n", node->nmspace.name);
    log_w_indent(out, indent, "{\n");

    for (mid_isize i = 0; i < node->nmspace.childs.len; ++i) {
        const struct MidParser_ASTNode *child = node->nmspace.childs.arr[i];
        MidParser_log_node(child, out, indent);
    }

    log_w_indent(out, indent, "} // namespace %s\n\n", node->nmspace.name);
}

static void log_var_inst(const struct MidParser_VarDeclInst *inst, FILE *out)
{
    char *type = MidParser_type_to_str(&inst->type);
    fprintf(out, "%s %s", type, inst->name);
    free(type);
    type = NULL;

    if (inst->has_ctor) {
        if (inst->ctor.ctor)
            fprintf(out, "#%" PRIi32 ":%" PRIi32,
                    MIDPARSER_GET_START(inst->ctor.ctor)->pos.line,
                    MIDPARSER_GET_START(inst->ctor.ctor)->pos.column);
        fprintf(out, "(");
        log_func_call_args(inst->ctor.args.arr, inst->ctor.args.len, out);
        fprintf(out, ")");
    } else if (inst->init.expr) {
        fprintf(out, " = ");
        log_expr(inst->init.expr, out);
    }
}

static void log_var_node(const struct MidParser_ASTNode *node, FILE *out,
                         int indent)
{
    log_w_indent(out, indent, "");

    for (mid_isize i = 0; i < node->var_decl.insts.len; ++i) {
        log_var_inst(node->var_decl.insts.arr[i], out);

        if (i + 1 < node->var_decl.insts.len) {
            fprintf(out, ", ");
        } else {
            fprintf(out, ";\n\n");
        }
    }
}

static void log_return_node(const struct MidParser_ASTNode *node, FILE *out,
                            int indent)
{
    log_w_indent(out, indent, "return");

    if (node->ret.expr) {
        fprintf(out, " ");
        log_expr(node->ret.expr, out);
    }

    fprintf(out, ";\n\n");
}

static void log_expr_node(const struct MidParser_ASTNode *node, FILE *out,
                          int indent)
{
    log_w_indent(out, indent, "");
    log_expr(&node->expr, out);
    fprintf(out, "\n\n");
}

void MidParser_log_node(const struct MidParser_ASTNode *node, FILE *out, int indent)
{
    if (node->type == MIDPARSER_ASTNODETYPE_FUNC_DECL)
        log_func_node(node, out, indent);
    else if (node->type == MIDPARSER_ASTNODETYPE_CLASS)
        log_class_node(node, out, indent);
    else if (node->type == MIDPARSER_ASTNODETYPE_NAMESPACE)
        log_namespace_node(node, out, indent);
    else if (node->type == MIDPARSER_ASTNODETYPE_VAR_DECL)
        log_var_node(node, out, indent);
    else if (node->type == MIDPARSER_ASTNODETYPE_RETURN)
        log_return_node(node, out, indent);
    else if (node->type == MIDPARSER_ASTNODETYPE_EXPR)
        log_expr_node(node, out, indent);
}

void MidParser_log_ast(const struct MidParser_ASTNode *root, FILE *out)
{
    assert(root->type == MIDPARSER_ASTNODETYPE_ROOT);

    for (mid_isize i = 0; i < root->root.len; ++i) {
        const struct MidParser_ASTNode *child = root->root.arr[i];
        MidParser_log_node(child, out, 0);
    }
}
