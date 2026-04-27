#include "ast.h"
#include "diag.h"
#include "ints.h"
#include "parser/expr.h"

void Parser_ASTNode_deinit(struct Parser_ASTNode *self)
{
    switch (self->type) {
    case PARSER_ASTNODETYPE_EXPR:
        Parser_Expr_deinit(&self->expr);
    }
}

struct Parser_ASTNode Parser_parse_node(const struct Lexer_Token *toks,
                                        isize_t start, isize_t end,
                                        struct DiagVec *diags)
{
    struct Parser_ASTNode ret;
    ret.type = PARSER_ASTNODETYPE_EXPR;
    ret.expr = Parser_parse_expr(toks, start, end, diags);

    return ret;
}
