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
#include "template.h"

// generic access macros

#define PARSER_GET_NODE_IMPL_MUT(node) ((struct Parser_ASTNode *)node)
#define PARSER_GET_NODE_IMPL_CONST(node) ((const struct Parser_ASTNode *)node)

#define PARSER_GET_NODE(node)                                                  \
    _Generic((node),                                                           \
        struct Parser_VarDecl *: PARSER_GET_NODE_IMPL_MUT(node),               \
        struct Parser_FuncDecl *: PARSER_GET_NODE_IMPL_MUT(node),              \
        struct Parser_Class *: PARSER_GET_NODE_IMPL_MUT(node),                 \
        struct Parser_Enum *: PARSER_GET_NODE_IMPL_MUT(node),                  \
        struct Parser_Namespace *: PARSER_GET_NODE_IMPL_MUT(node),             \
        struct Parser_Return *: PARSER_GET_NODE_IMPL_MUT(node),                \
        struct Parser_Tmplt *: PARSER_GET_NODE_IMPL_MUT(node),                 \
        struct Parser_TmpltParam *: PARSER_GET_NODE_IMPL_MUT(node),            \
        const struct Parser_VarDecl *: PARSER_GET_NODE_IMPL_CONST(node),       \
        const struct Parser_FuncDecl *: PARSER_GET_NODE_IMPL_CONST(node),      \
        const struct Parser_Class *: PARSER_GET_NODE_IMPL_CONST(node),         \
        const struct Parser_Enum *: PARSER_GET_NODE_IMPL_CONST(node),          \
        const struct Parser_Namespace *: PARSER_GET_NODE_IMPL_CONST(node),     \
        const struct Parser_Return *: PARSER_GET_NODE_IMPL_CONST(node),        \
        const struct Parser_Tmplt *: PARSER_GET_NODE_IMPL_CONST(node),         \
        const struct Parser_TmpltParam *: PARSER_GET_NODE_IMPL_CONST(node))

#define PARSER_GET_PARENT(node) (PARSER_GET_NODE(node)->parent)
#define PARSER_GET_START(node) (PARSER_GET_NODE(node)->start)
#define PARSER_GET_TYPE(node) (PARSER_GET_NODE(node)->type)

enum Parser_ASTNodeType {
    PARSER_ASTNODETYPE_ROOT,
    PARSER_ASTNODETYPE_EXPR,
    PARSER_ASTNODETYPE_VAR_DECL,
    PARSER_ASTNODETYPE_FUNC_DECL,
    PARSER_ASTNODETYPE_CLASS,
    PARSER_ASTNODETYPE_ENUM,
    PARSER_ASTNODETYPE_NAMESPACE,
    PARSER_ASTNODETYPE_RETURN,
    PARSER_ASTNODETYPE_TMPLT,
    PARSER_ASTNODETYPE_TMPLT_PARAM,
};

struct Parser_ASTNode {
    // NOTE: THIS UNION MUST GO FIRST TO ALLOW CASTING BETWEEN POINTER TYPES
    union {
        struct Parser_ASTNodePVec root;
        struct Parser_Expr expr;
        struct Parser_VarDecl var_decl;
        struct Parser_FuncDecl func_decl;
        struct Parser_Class class_;
        struct Parser_Enum enum_;
        struct Parser_Namespace nmspace;
        struct Parser_Return ret;
        struct Parser_Tmplt tmplt;
        struct Parser_TmpltParam tmplt_param;
    };
    struct Parser_ASTNode *parent;
    const struct Lexer_Token *start;
    enum Parser_ASTNodeType type;
};

void Parser_ASTNode_deinit(struct Parser_ASTNode *self);
void Parser_copy_node(struct Parser_ASTNode *dest,
                      const struct Parser_ASTNode *src,
                      struct Parser_ASTNode *dest_parent,
                      struct Sema_Scope *dest_scope,
                      struct Parser_Allocators *allocs);

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
// is the node a template, example:
//    template <typename T> void func(T arg);
//                          ^
//                        node
bool Parser_node_is_templated(const struct Parser_ASTNode *node);
