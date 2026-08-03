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

#ifdef __cplusplus
extern "C" {
#endif

// generic access macros

#define MIDPAR_GET_NODE_IMPL_MUT(node) ((struct midpar_ASTNode *)node)
#define MIDPAR_GET_NODE_IMPL_CONST(node) ((const struct midpar_ASTNode *)node)

/*
 * generic macro to convert a piece of syntax to a generic midpar_ASTNode
 * ptr. gives a compile time error if you use an invalid type. preserves
 * const-ness.
 */
#define MIDPAR_GET_NODE(node)                                                  \
    _Generic((node),                                                           \
        struct midpar_VarDecl *: MIDPAR_GET_NODE_IMPL_MUT(node),               \
        struct midpar_VarDeclInst *: MIDPAR_GET_NODE_IMPL_MUT(node),           \
        struct midpar_FuncDecl *: MIDPAR_GET_NODE_IMPL_MUT(node),              \
        struct midpar_Class *: MIDPAR_GET_NODE_IMPL_MUT(node),                 \
        struct midpar_Enum *: MIDPAR_GET_NODE_IMPL_MUT(node),                  \
        struct midpar_Namespace *: MIDPAR_GET_NODE_IMPL_MUT(node),             \
        struct midpar_Return *: MIDPAR_GET_NODE_IMPL_MUT(node),                \
        struct midpar_Tmplt *: MIDPAR_GET_NODE_IMPL_MUT(node),                 \
        struct midpar_TmpltParam *: MIDPAR_GET_NODE_IMPL_MUT(node),            \
        struct midpar_TmpltNonTypeParam *: MIDPAR_GET_NODE_IMPL_MUT(node),     \
        struct midpar_TmpltTypeParam *: MIDPAR_GET_NODE_IMPL_MUT(node),        \
        struct midpar_TmpltTmpltParam *: MIDPAR_GET_NODE_IMPL_MUT(node),       \
        const struct midpar_VarDecl *: MIDPAR_GET_NODE_IMPL_CONST(node),       \
        const struct midpar_VarDeclInst *: MIDPAR_GET_NODE_IMPL_CONST(node),   \
        const struct midpar_FuncDecl *: MIDPAR_GET_NODE_IMPL_CONST(node),      \
        const struct midpar_Class *: MIDPAR_GET_NODE_IMPL_CONST(node),         \
        const struct midpar_Enum *: MIDPAR_GET_NODE_IMPL_CONST(node),          \
        const struct midpar_Namespace *: MIDPAR_GET_NODE_IMPL_CONST(node),     \
        const struct midpar_Return *: MIDPAR_GET_NODE_IMPL_CONST(node),        \
        const struct midpar_Tmplt *: MIDPAR_GET_NODE_IMPL_CONST(node),         \
        const struct midpar_TmpltParam *: MIDPAR_GET_NODE_IMPL_CONST(node),    \
        const struct midpar_TmpltNonTypeParam *: MIDPAR_GET_NODE_IMPL_CONST(   \
                 node),                                                        \
        const struct midpar_TmpltTypeParam *: MIDPAR_GET_NODE_IMPL_CONST(      \
                 node),                                                        \
        const struct midpar_TmpltTmpltParam *: MIDPAR_GET_NODE_IMPL_CONST(     \
                 node))

#define MIDPAR_GET_PARENT(node) (MIDPAR_GET_NODE(node)->parent)
#define MIDPAR_GET_START(node) (MIDPAR_GET_NODE(node)->start)
#define MIDPAR_GET_TYPE(node) (MIDPAR_GET_NODE(node)->type)

enum midpar_ASTNodeType {
    MIDPAR_ASTNODETYPE_ROOT,
    MIDPAR_ASTNODETYPE_EXPR,
    MIDPAR_ASTNODETYPE_VAR_DECL,
    MIDPAR_ASTNODETYPE_VAR_DECL_INST,
    MIDPAR_ASTNODETYPE_FUNC_DECL,
    MIDPAR_ASTNODETYPE_CLASS,
    MIDPAR_ASTNODETYPE_ENUM,
    MIDPAR_ASTNODETYPE_NAMESPACE,
    MIDPAR_ASTNODETYPE_RETURN,
    MIDPAR_ASTNODETYPE_TMPLT,
    MIDPAR_ASTNODETYPE_TMPLT_PARAM,
};

struct midpar_ASTNode {
    // NOTE: THIS UNION MUST GO FIRST TO ALLOW CASTING BETWEEN POINTER TYPES
    union {
        struct midpar_ASTNodePVec root;
        struct midpar_Expr expr;
        struct midpar_VarDecl var_decl;
        struct midpar_VarDeclInst var_inst;
        struct midpar_FuncDecl func_decl;
        struct midpar_Class class_;
        struct midpar_Enum enum_;
        struct midpar_Namespace nmspace;
        struct midpar_Return ret;
        struct midpar_Tmplt tmplt;
        struct midpar_TmpltParam tmplt_param;
    };
    struct midpar_ASTNode *parent;
    const struct midlex_Token *start;
    enum midpar_ASTNodeType type;
};

void midpar_ASTNode_deinit(struct midpar_ASTNode *self);
void midpar_copy_node(struct midpar_ASTNode *dest,
                      const struct midpar_ASTNode *src,
                      struct midpar_ASTNode *dest_parent,
                      struct midsema_Scope *dest_scope,
                      struct midpar_Allocators *allocs);

struct midpar_ParseNodeFlags {
    bool skip_def;
    bool is_field; // is the node a field of a class.
                   // parent is assumed to be the parent class.
};
// skip_def - if true and the node has a definition / initializer, then it
//            won't be parsed and rather be skipped
struct midpar_ASTNode *midpar_parse_node(const struct midlex_Token *toks,
                                         mid_isize start, mid_isize *out_end,
                                         struct midpar_ASTNode *parent,
                                         struct midsema_Scope *scope,
                                         struct midpar_ParseNodeFlags flags,
                                         struct midpar_Allocators *allocs,
                                         struct mid_DiagVec *diags);
// is the node a template, example:
//    template <typename T> void func(T arg);
//                          ^
//                        node
bool midpar_node_is_templated(const struct midpar_ASTNode *node);

#ifdef __cplusplus
}
#endif
