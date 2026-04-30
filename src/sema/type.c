#include "type.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/type.h"
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

static const struct Parser_ASTNode *
find_type_impl(const char *name, const struct Parser_ASTNode *node,
               const struct Lexer_Token *end)
{
    auto subs = Parser_node_subs_const(node);
    if (!subs)
        return NULL;

    for (isize_t i = 0; i < subs->len; ++i) {
        if (subs->arr[i].start >= end)
            break;

        if (Sema_node_is_type(&subs->arr[i]) &&
            !strcmp(Sema_node_type_name(&subs->arr[i]), name))
            return &subs->arr[i];
    }

    if (node->parent)
        return find_type_impl(name, node->parent, end);
    else
        return NULL;
}

const struct Parser_ASTNode *
Sema_find_type_const(const char *name, const struct Parser_ASTNode *node,
                     const struct Lexer_Token *end)
{
    return find_type_impl(name, node, end);
}

struct Parser_ASTNode *Sema_find_type(const char *name,
                                      struct Parser_ASTNode *node,
                                      const struct Lexer_Token *end)
{
    return (struct Parser_ASTNode *)Sema_find_type_const(name, node, end);
}
