#include "type.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ident.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/expr.h"
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
        Sema_ident_type_const(expr->tok->ident, parent, expr->tok);

    if (!type)
        gen_dynpush(diags,
                    ((struct Diag){
                        .pos = expr->tok->pos,
                        .line = expr->tok->line,
                        .msg = Print_fmt_to_str("undeclared identifier '%s'",
                                                expr->tok->ident),
                        .err = ERRORTYPE_UNDECLARED_IDENTIFIER,
                        .is_err = true,
                    }));
    else
        expr->ret = Parser_copy_type(type);
}

static void set_func_call_node(struct Parser_Expr *expr,
                               struct Parser_ASTNode *parent,
                               struct DiagVec *diags)
{
    const struct Parser_Expr *name_expr = &expr->info.args.arr[0];

    if (name_expr->type == PARSER_EXPRTYPE_IDENTIFIER) {
        expr->node =
            Sema_ident_creation(name_expr->info.ident, parent, expr->tok);
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

static void typecheck_func_call(struct Parser_Expr *expr,
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

        if (expr->type == PARSER_EXPRTYPE_FUNC_CALL)
            typecheck_func_call(expr, parent, diags);
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
