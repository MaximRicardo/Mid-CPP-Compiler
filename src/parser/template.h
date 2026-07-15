#pragma once

#include "astvec.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/expr.h"
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
    struct Parser_ASTNode *def_arg;
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

enum Parser_TmpltArgType {
    PARSER_TMPLTARG_EXPR,  // used by nontype params
    PARSER_TMPLTARG_TYPE,  // used by type params
    PARSER_TMPLTARG_IDENT, // used by template template params
};

struct Parser_TmpltArg {
    union {
        struct Parser_Expr expr;
        struct Parser_Type type;
        struct Sema_Ident *ident;
    };
    enum Parser_TmpltArgType kind;
};
gen_dynarray_struct_named(Parser_TmpltArgVec, struct Parser_TmpltArg);

void Parser_TmpltArg_deinit(struct Parser_TmpltArg *self);

isize_t Parser_parse_tmplt(struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags);
const struct Sema_Ident *
Parser_tmplt_ident_const(const struct Parser_Tmplt *self);
struct Sema_Ident *Parser_tmplt_ident(struct Parser_Tmplt *self);
struct Parser_TmpltArgVec
Parser_parse_tmplt_args(const struct Lexer_Token *toks, isize_t l_angle,
                        isize_t *out_r_angle, struct Sema_Scope *scope,
                        struct DiagVec *diags);
