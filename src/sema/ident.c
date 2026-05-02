#include "ident.h"
#include "generics/dynarray.h"
#include "ints.h"
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
Sema_ident_type_const(const char *ident, const struct Parser_ASTNode *node)
{
    {
        auto type = Sema_node_creates_ident_const(node, ident);
        if (type)
            return type;
    }

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            const struct Parser_Type *type =
                Sema_node_creates_ident_const(subs->arr[i], ident);
            if (type)
                return type;
        }
    }

    if (node->parent)
        return Sema_ident_type_const(ident, node->parent);
    else
        return NULL;
}

struct Parser_Type *Sema_ident_type(const char *ident,
                                    struct Parser_ASTNode *node)
{
    return (struct Parser_Type *)Sema_ident_type_const(ident, node);
}

const struct Parser_ASTNode *
Sema_ident_creation_const(const char *ident, const struct Parser_ASTNode *node)
{
    if (Sema_node_creates_ident_const(node, ident))
        return node;

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (Sema_node_creates_ident_const(subs->arr[i], ident))
                return subs->arr[i];
        }
    }

    if (node->parent)
        return Sema_ident_creation_const(ident, node->parent);
    else
        return NULL;
}

struct Parser_ASTNode *Sema_ident_creation(const char *ident,
                                           struct Parser_ASTNode *node)
{
    return (struct Parser_ASTNode *)Sema_ident_creation_const(ident, node);
}

static bool node_is_func_def(const struct Parser_ASTNode *node,
                             const char *name)
{
    return node->type == PARSER_ASTNODETYPE_FUNC_DECL &&
           node->func_decl.has_def && !strcmp(node->func_decl.name, name);
}

static bool node_is_class_def(const struct Parser_ASTNode *node,
                              const char *name)
{
    return node->type == PARSER_ASTNODETYPE_CLASS && node->class_.has_def &&
           !strcmp(node->class_.name, name);
}

static bool node_is_ident_def(const struct Parser_ASTNode *node,
                              const char *name)
{
    return node_is_func_def(node, name) || node_is_class_def(node, name);
}

const struct Parser_ASTNode *
Sema_ident_def_const(const char *name, const struct Parser_ASTNode *node)
{
    if (node_is_ident_def(node, name))
        return node;

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (node_is_ident_def(subs->arr[i], name))
                return subs->arr[i];
        }
    }

    if (node->parent)
        return Sema_ident_def_const(name, node->parent);
    else
        return NULL;
}

struct Parser_ASTNode *Sema_ident_def(const char *name,
                                      struct Parser_ASTNode *node)
{
    return (struct Parser_ASTNode *)Sema_ident_def_const(name, node);
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
                            struct Parser_ASTNodePVec *result)
{
    if (node_is_op_overload(node, op))
        gen_dynpush(result, node);

    auto subs = Parser_node_subs_const(node);
    if (subs) {
        for (isize_t i = 0; i < subs->len; ++i) {
            if (node_is_op_overload(subs->arr[i], op))
                gen_dynpush(result, subs->arr[i]);
        }
    }

    if (node->parent)
        find_op_overloads_impl(op, node->parent, result);
}

struct Parser_ASTNodePVec Sema_op_overloads(enum Parser_ExprType op,
                                            struct Parser_ASTNode *node)
{
    struct Parser_ASTNodePVec ret = {};
    find_op_overloads_impl(op, node, &ret);
    return ret;
}
