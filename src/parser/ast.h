#pragma once

#include "class.h"
#include "diag.h"
#include "enum.h"
#include "expr.h"
#include "ints.h"
#include "lexer/token.h"
#include "namespace.h"
#include "parser/astvec.h"
#include "parser/func_decl.h"
#include "parser/var_decl.h"

enum Parser_ASTNodeType {
    PARSER_ASTNODETYPE_ROOT,
    PARSER_ASTNODETYPE_EXPR,
    PARSER_ASTNODETYPE_VAR_DECL,
    PARSER_ASTNODETYPE_FUNC_DECL,
    PARSER_ASTNODETYPE_CLASS,
    PARSER_ASTNODETYPE_ENUM,
    PARSER_ASTNODETYPE_NAMESPACE,
};

struct Parser_ASTNode {
    union {
        struct Parser_ASTNodePVec root;
        struct Parser_Expr expr;
        struct Parser_VarDecl var_decl;
        struct Parser_FuncDecl func_decl;
        struct Parser_Class class_;
        struct Parser_Enum enum_;
        struct Parser_Namespace nmspace;
    };
    struct Parser_ASTNode *parent;
    const struct Lexer_Token *start;
    enum Parser_ASTNodeType type;
};

void Parser_ASTNode_deinit(struct Parser_ASTNode *self);
void Parser_ASTNodeP_deinit(struct Parser_ASTNode **self);
// skip_def - if true and the node has a definition / initializer, then it
//            won't be parsed and rather be skipped
struct Parser_ASTNode *Parser_parse_node(const struct Lexer_Token *toks,
                                         isize_t start, isize_t *out_end,
                                         struct Parser_ASTNode *parent,
                                         bool skip_def, struct DiagVec *diags);
// returns NULL if the node doesn't have a vector of children
const struct Parser_ASTNodePVec *
Parser_node_subs_const(const struct Parser_ASTNode *node);
struct Parser_ASTNodePVec *Parser_node_subs(struct Parser_ASTNode *node);
