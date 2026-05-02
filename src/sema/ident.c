#include "ident.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/type.h"
#include <stdio.h>
#include <string.h>

bool Sema_node_creates_idents(const struct Parser_ASTNode *node)
{
    return (node->type == PARSER_ASTNODETYPE_VAR_DECL &&
            !node->var_decl.type.squals.is_typedef) ||
           node->type == PARSER_ASTNODETYPE_FUNC_DECL;
}

const struct Parser_Type *
Sema_node_creates_ident_const(const struct Parser_ASTNode *node,
                              const char *ident)
{
    if (node->type == PARSER_ASTNODETYPE_VAR_DECL) {
        if (!strcmp(node->var_decl.name, ident))
            return &node->var_decl.type;
    } else if (node->type == PARSER_ASTNODETYPE_FUNC_DECL) {
        if (!strcmp(node->func_decl.name, ident))
            return &node->func_decl.type;

        for (isize_t i = 0; i < node->func_decl.params.len; ++i) {
            if (!strcmp(node->func_decl.params.arr[i].name, ident))
                return &node->func_decl.params.arr[i].type;
        }
    }

    return NULL;
}

struct Parser_Type *Sema_node_creates_ident(struct Parser_ASTNode *node,
                                            const char *ident)
{
    return (struct Parser_Type *)Sema_node_creates_ident_const(node, ident);
}

const struct Parser_Type *
Sema_ident_type_const(const char *ident, const struct Parser_ASTNode *node,
                      const struct Lexer_Token *end)
{
    {
        auto type = Sema_node_creates_ident_const(node, ident);
        if (type)
            return type;
    }

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (end && subs->arr[i]->start >= end)
                break;

            const struct Parser_Type *type =
                Sema_node_creates_ident_const(subs->arr[i], ident);
            if (type)
                return type;
        }
    }

    if (node->parent)
        return Sema_ident_type_const(ident, node->parent, end);
    else
        return NULL;
}

struct Parser_Type *Sema_ident_type(const char *ident,
                                    struct Parser_ASTNode *node,
                                    const struct Lexer_Token *end)
{
    return (struct Parser_Type *)Sema_ident_type_const(ident, node, end);
}

const struct Parser_ASTNode *
Sema_ident_creation_const(const char *ident, const struct Parser_ASTNode *node,
                          const struct Lexer_Token *end)
{
    if (Sema_node_creates_ident_const(node, ident))
        return node;

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (end && subs->arr[i]->start >= end)
                break;

            if (Sema_node_creates_ident_const(subs->arr[i], ident))
                return subs->arr[i];
        }
    }

    if (node->parent)
        return Sema_ident_creation_const(ident, node->parent, end);
    else
        return NULL;
}

struct Parser_ASTNode *Sema_ident_creation(const char *ident,
                                           struct Parser_ASTNode *node,
                                           const struct Lexer_Token *end)
{
    return (struct Parser_ASTNode *)Sema_ident_creation_const(ident, node, end);
}

static bool node_is_func_def(const struct Parser_ASTNode *node,
                             const char *func)
{
    return node->type == PARSER_ASTNODETYPE_FUNC_DECL &&
           node->func_decl.has_def && !strcmp(node->func_decl.name, func);
}

const struct Parser_ASTNode *
Sema_func_def_const(const char *name, const struct Parser_ASTNode *node,
                    const struct Lexer_Token *end)
{
    if (node_is_func_def(node, name))
        return node;

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (end && subs->arr[i]->start >= end)
                break;

            if (node_is_func_def(subs->arr[i], name))
                return subs->arr[i];
        }
    }

    if (node->parent)
        return Sema_ident_creation_const(name, node->parent, end);
    else
        return NULL;
}

struct Parser_ASTNode *Sema_func_def(const char *name,
                                     struct Parser_ASTNode *node,
                                     const struct Lexer_Token *end)
{
    return (struct Parser_ASTNode *)Sema_func_def_const(name, node, end);
}

static bool node_is_op_overload(const struct Parser_ASTNode *node,
                                enum Parser_ExprType op)
{
    return node->type == PARSER_ASTNODETYPE_FUNC_DECL &&
           node->func_decl.is_op_overload &&
           node->func_decl.op_overload == op &&
           !strcmp(node->func_decl.name, "operator");
}

void find_op_overloads_impl(enum Parser_ExprType op,
                            struct Parser_ASTNode *node,
                            const struct Lexer_Token *end,
                            struct Parser_ASTNodePVec *result)
{
    if (node_is_op_overload(node, op))
        gen_dynpush(result, node);

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (end && subs->arr[i]->start >= end)
                break;

            if (node_is_op_overload(subs->arr[i], op))
                gen_dynpush(result, subs->arr[i]);
        }
    }

    if (node->parent)
        find_op_overloads_impl(op, node->parent, end, result);
}

struct Parser_ASTNodePVec Sema_op_overloads(enum Parser_ExprType op,
                                            struct Parser_ASTNode *node,
                                            const struct Lexer_Token *end)
{
    struct Parser_ASTNodePVec ret = {};
    find_op_overloads_impl(op, node, end, &ret);
    return ret;
}
