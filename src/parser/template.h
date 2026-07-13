#pragma once

#include "astvec.h"
#include "diag.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/type.h"
#include "sema/scope.h"

struct Parser_Tmplt {
    struct Parser_ASTNodePVec params; // of type PARSER_ASTNODETYPE_TMPLT_PARAM
    struct Parser_ASTNode *child;
    struct Sema_Scope *scope;
};

void Parser_Tmplt_deinit(struct Parser_Tmplt *self);

enum Parser_TmpltParamType {
    PARSER_TMPLTPARAM_NONTYPE,
    PARSER_TMPLTPARAM_TYPE,
    PARSER_TMPLTPARAM_TMPLT,
};

struct Parser_TmpltNonTypeParam {
    struct Parser_Type type;
    const char *name;
    struct Parser_Expr *def_arg;
    struct Parser_ASTNode *parent;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void Parser_TmpltNonTypeParam_deinit(struct Parser_TmpltNonTypeParam *self);

struct Parser_TmpltTypeParam {
    const char *name;
    struct Parser_Type *def_arg;
    struct Parser_ASTNode *parent;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void Parser_TmpltTypeParam_deinit(struct Parser_TmpltTypeParam *self);

struct Parser_TmpltTmpltParam {
    struct Parser_ASTNode *tmplt;
    const char *name;
    struct Parser_Type *def_arg; // must have a matching template
    struct Parser_ASTNode *parent;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void Parser_TmpltTmpltParam_deinit(struct Parser_TmpltTmpltParam *self);

struct Parser_TmpltParam {
    union {
        struct Parser_TmpltNonTypeParam non_type;
        struct Parser_TmpltTypeParam type;
        struct Parser_TmpltTmpltParam tmplt;
    };
    enum Parser_TmpltParamType kind;
};

void Parser_TmpltParam_deinit(struct Parser_TmpltParam *self);

isize_t Parser_parse_tmplt(struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags);
