#pragma once

#include "diag.h"
#include "expr.h"
#include "ints.h"
#include "lexer/token.h"

enum Parser_ASTNodeType {
    PARSER_ASTNODETYPE_EXPR,
};

struct Parser_ASTNode {
    union {
        struct Parser_Expr expr;
    };
    enum Parser_ASTNodeType type;
};

void Parser_ASTNode_deinit(struct Parser_ASTNode *self);
struct Parser_ASTNode Parser_parse_node(const struct Lexer_Token *toks,
                                        isize_t start, isize_t end,
                                        struct DiagVec *diags);
