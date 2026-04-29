#include "ast.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/enum.h"
#include "parser/expr.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/type.h"

void Parser_ASTNode_deinit(struct Parser_ASTNode *self)
{
    switch (self->type) {
    case PARSER_ASTNODETYPE_ROOT:
        gen_dyndeinit(&self->root, Parser_ASTNode_deinit);
        break;

    case PARSER_ASTNODETYPE_EXPR:
        Parser_Expr_deinit(&self->expr);
        break;

    case PARSER_ASTNODETYPE_VAR_DECL:
        Parser_VarDecl_deinit(&self->var_decl);
        break;

    case PARSER_ASTNODETYPE_ENUM:
        Parser_Enum_deinit(&self->enum_);
        break;

    case PARSER_ASTNODETYPE_CLASS:
        Parser_Class_deinit(&self->class_);
        break;

    case PARSER_ASTNODETYPE_NAMESPACE:
        Parser_Namespace_deinit(&self->nmspace);
        break;
    }
}

static struct Diag missing_semi_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("missing semicolon"),
        .err = ERRORTYPE_MISSING_SEMICOLON,
        .is_err = true,
    };
}

struct Parser_ASTNode Parser_parse_node(const struct Lexer_Token *toks,
                                        isize_t start, isize_t *out_end,
                                        struct Parser_ASTNode *parent,
                                        struct DiagVec *diags)
{
    struct Parser_ASTNode ret = {};
    ret.start = &toks[start];
    ret.parent = parent;

    isize_t end;

    if (Lexer_is_typemod(toks[start].type) ||
        Lexer_is_typequal(toks[start].type) ||
        Sema_is_typespec(&toks[start], parent)) {
        ret.type = PARSER_ASTNODETYPE_VAR_DECL;
        ret.var_decl = Parser_parse_var_decl(toks, start, &end, parent, diags);
    } else {
        ret.type = PARSER_ASTNODETYPE_EXPR;
        ret.expr = Parser_parse_expr(toks, start, LEXER_TOKENTYPE_SEMICOLON,
                                     &end, diags);
    }

    if (toks[end].type != LEXER_TOKENTYPE_SEMICOLON)
        gen_dynpush(diags, missing_semi_err(&toks[start]));
    else
        ++end;

    if (out_end)
        *out_end = end;
    return ret;
}

const struct Parser_ASTNodeVec *
Parser_node_subs_const(const struct Parser_ASTNode *node)
{
    switch (node->type) {
    case PARSER_ASTNODETYPE_ROOT:
        return &node->root;

    case PARSER_ASTNODETYPE_ENUM:
        return &node->enum_.nodes;

    case PARSER_ASTNODETYPE_CLASS:
        return &node->class_.nodes;

    case PARSER_ASTNODETYPE_NAMESPACE:
        return &node->nmspace.nodes;

    default:
        return NULL;
    }
}

struct Parser_ASTNodeVec *Parser_node_subs(struct Parser_ASTNode *node)
{
    return (struct Parser_ASTNodeVec *)Parser_node_subs_const(node);
}
