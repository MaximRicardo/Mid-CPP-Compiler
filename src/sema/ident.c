#include "ident.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/ast.h"
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
            if (subs->arr[i]->start >= end)
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
            if (subs->arr[i]->start >= end)
                break;

            if (Sema_node_creates_ident_const(subs->arr[i], ident))
                return node;
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
