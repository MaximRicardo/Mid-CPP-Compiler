#pragma once

#include "allocator.h"
#include "class.h"
#include "diag.h"
#include "enum.h"
#include "expr.h"
#include "ints.h"
#include "lexer/token.h"
#include "namespace.h"
#include "parser/astvec.h"
#include "parser/func_decl.h"
#include "parser/return.h"
#include "parser/var_decl.h"
#include "sema/scope.h"

enum Parser_ASTNodeType {
    PARSER_ASTNODETYPE_ROOT,
    PARSER_ASTNODETYPE_EXPR,
    PARSER_ASTNODETYPE_VAR_DECL,
    PARSER_ASTNODETYPE_FUNC_DECL,
    PARSER_ASTNODETYPE_CLASS,
    PARSER_ASTNODETYPE_ENUM,
    PARSER_ASTNODETYPE_NAMESPACE,
    PARSER_ASTNODETYPE_RETURN,
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
        struct Parser_Return ret;
    };
    struct Parser_ASTNode *parent;
    const struct Lexer_Token *start;
    enum Parser_ASTNodeType type;
};

void Parser_ASTNode_deinit(struct Parser_ASTNode *self);

struct Parser_ParseNodeFlags {
    bool skip_def;
    bool is_field; // is the node a field of a class.
                   // parent is assumed to be the parent class.
};
// skip_def - if true and the node has a definition / initializer, then it
//            won't be parsed and rather be skipped
struct Parser_ASTNode *
Parser_parse_node(const struct Lexer_Token *toks, isize_t start,
                  isize_t *out_end, struct Parser_ASTNode *parent,
                  struct Sema_Scope *scope, struct Parser_ParseNodeFlags flags,
                  struct Parser_Allocators *allocs, struct DiagVec *diags);
