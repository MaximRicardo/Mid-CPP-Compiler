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

#define MIDPARSER_GET_NODE_IMPL_MUT(node) ((struct MidParser_ASTNode *)node)
#define MIDPARSER_GET_NODE_IMPL_CONST(node) ((const struct MidParser_ASTNode *)node)

/*
 * generic macro to convert a piece of syntax to a generic MidParser_ASTNode ptr.
 * gives a compile time error if you use an invalid type.
 * preserves const-ness.
 */
#define MIDPARSER_GET_NODE(node)                                                  \
    _Generic((node),                                                           \
        struct MidParser_VarDecl *: MIDPARSER_GET_NODE_IMPL_MUT(node),               \
        struct MidParser_VarDeclInst *: MIDPARSER_GET_NODE_IMPL_MUT(node),           \
        struct MidParser_FuncDecl *: MIDPARSER_GET_NODE_IMPL_MUT(node),              \
        struct MidParser_Class *: MIDPARSER_GET_NODE_IMPL_MUT(node),                 \
        struct MidParser_Enum *: MIDPARSER_GET_NODE_IMPL_MUT(node),                  \
        struct MidParser_Namespace *: MIDPARSER_GET_NODE_IMPL_MUT(node),             \
        struct MidParser_Return *: MIDPARSER_GET_NODE_IMPL_MUT(node),                \
        struct MidParser_Tmplt *: MIDPARSER_GET_NODE_IMPL_MUT(node),                 \
        struct MidParser_TmpltParam *: MIDPARSER_GET_NODE_IMPL_MUT(node),            \
        struct MidParser_TmpltNonTypeParam *: MIDPARSER_GET_NODE_IMPL_MUT(node),     \
        struct MidParser_TmpltTypeParam *: MIDPARSER_GET_NODE_IMPL_MUT(node),        \
        struct MidParser_TmpltTmpltParam *: MIDPARSER_GET_NODE_IMPL_MUT(node),       \
        const struct MidParser_VarDecl *: MIDPARSER_GET_NODE_IMPL_CONST(node),       \
        const struct MidParser_VarDeclInst *: MIDPARSER_GET_NODE_IMPL_CONST(node),   \
        const struct MidParser_FuncDecl *: MIDPARSER_GET_NODE_IMPL_CONST(node),      \
        const struct MidParser_Class *: MIDPARSER_GET_NODE_IMPL_CONST(node),         \
        const struct MidParser_Enum *: MIDPARSER_GET_NODE_IMPL_CONST(node),          \
        const struct MidParser_Namespace *: MIDPARSER_GET_NODE_IMPL_CONST(node),     \
        const struct MidParser_Return *: MIDPARSER_GET_NODE_IMPL_CONST(node),        \
        const struct MidParser_Tmplt *: MIDPARSER_GET_NODE_IMPL_CONST(node),         \
        const struct MidParser_TmpltParam *: MIDPARSER_GET_NODE_IMPL_CONST(node),    \
        const struct MidParser_TmpltNonTypeParam *: MIDPARSER_GET_NODE_IMPL_CONST(   \
                 node),                                                        \
        const struct MidParser_TmpltTypeParam *: MIDPARSER_GET_NODE_IMPL_CONST(      \
                 node),                                                        \
        const struct MidParser_TmpltTmpltParam *: MIDPARSER_GET_NODE_IMPL_CONST(     \
                 node))

#define MIDPARSER_GET_PARENT(node) (MIDPARSER_GET_NODE(node)->parent)
#define MIDPARSER_GET_START(node) (MIDPARSER_GET_NODE(node)->start)
#define MIDPARSER_GET_TYPE(node) (MIDPARSER_GET_NODE(node)->type)

enum MidParser_ASTNodeType {
    MIDPARSER_ASTNODETYPE_ROOT,
    MIDPARSER_ASTNODETYPE_EXPR,
    MIDPARSER_ASTNODETYPE_VAR_DECL,
    MIDPARSER_ASTNODETYPE_VAR_DECL_INST,
    MIDPARSER_ASTNODETYPE_FUNC_DECL,
    MIDPARSER_ASTNODETYPE_CLASS,
    MIDPARSER_ASTNODETYPE_ENUM,
    MIDPARSER_ASTNODETYPE_NAMESPACE,
    MIDPARSER_ASTNODETYPE_RETURN,
    MIDPARSER_ASTNODETYPE_TMPLT,
    MIDPARSER_ASTNODETYPE_TMPLT_PARAM,
};

struct MidParser_ASTNode {
    // NOTE: THIS UNION MUST GO FIRST TO ALLOW CASTING BETWEEN POINTER TYPES
    union {
        struct MidParser_ASTNodePVec root;
        struct MidParser_Expr expr;
        struct MidParser_VarDecl var_decl;
        struct MidParser_VarDeclInst var_inst;
        struct MidParser_FuncDecl func_decl;
        struct MidParser_Class class_;
        struct MidParser_Enum enum_;
        struct MidParser_Namespace nmspace;
        struct MidParser_Return ret;
        struct MidParser_Tmplt tmplt;
        struct MidParser_TmpltParam tmplt_param;
    };
    struct MidParser_ASTNode *parent;
    const struct MidLexer_Token *start;
    enum MidParser_ASTNodeType type;
};

void MidParser_ASTNode_deinit(struct MidParser_ASTNode *self);
void MidParser_copy_node(struct MidParser_ASTNode *dest,
                      const struct MidParser_ASTNode *src,
                      struct MidParser_ASTNode *dest_parent,
                      struct MidSema_Scope *dest_scope,
                      struct MidParser_Allocators *allocs);

struct MidParser_ParseNodeFlags {
    bool skip_def;
    bool is_field; // is the node a field of a class.
                   // parent is assumed to be the parent class.
};
// skip_def - if true and the node has a definition / initializer, then it
//            won't be parsed and rather be skipped
struct MidParser_ASTNode *
MidParser_parse_node(const struct MidLexer_Token *toks, mid_isize start,
                  mid_isize *out_end, struct MidParser_ASTNode *parent,
                  struct MidSema_Scope *scope, struct MidParser_ParseNodeFlags flags,
                  struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags);
// is the node a template, example:
//    template <typename T> void func(T arg);
//                          ^
//                        node
bool MidParser_node_is_templated(const struct MidParser_ASTNode *node);
