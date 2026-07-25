#pragma once

#include "astvec.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/scope.h"

struct Parser_TmpltInst {
    struct Parser_ASTNode *inst;
    struct Sema_Scope *scope; // the inst node's parent scope, which is a child
                              // of the template scope
};
gen_dynarray_struct_named(Parser_TmpltInstVec, struct Parser_TmpltInst);

struct Parser_Tmplt {
    struct Parser_ASTNodePVec params; // of type PARSER_ASTNODETYPE_TMPLT_PARAM
    struct Parser_ASTNode *child;
    struct Sema_Scope *scope;
};

void Parser_Tmplt_deinit(struct Parser_Tmplt *self);
void Parser_copy_tmplt(struct Parser_ASTNode *dest,
                       const struct Parser_ASTNode *src,
                       struct Sema_Scope *dest_scope,
                       struct Parser_Allocators *allocs);

enum Parser_TmpltParamType {
    PARSER_TMPLTPARAM_NONTYPE,
    PARSER_TMPLTPARAM_TYPE,
    PARSER_TMPLTPARAM_TMPLT,
};

struct Parser_TmpltNonTypeParam {
    struct Parser_Type type;
    const char *name;
    struct Parser_Expr *def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void Parser_TmpltNonTypeParam_deinit(struct Parser_TmpltNonTypeParam *self);

struct Parser_TmpltTypeParam {
    const char *name;
    struct Parser_Type *def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void Parser_TmpltTypeParam_deinit(struct Parser_TmpltTypeParam *self);

struct Parser_TmpltTmpltParam {
    struct Parser_ASTNode *tmplt;
    const char *name;
    struct Sema_IdentPtr def_arg;
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
void Parser_copy_tmplt_param(struct Parser_ASTNode *dest,
                             const struct Parser_ASTNode *src,
                             struct Parser_Allocators *allocs);

enum Parser_TmpltArgType {
    PARSER_TMPLTARG_NONTYPE,
    PARSER_TMPLTARG_TYPE,
    PARSER_TMPLTARG_TMPLT,
};

struct Parser_TmpltArg {
    union {
        struct Parser_Expr non_type;
        struct Parser_Type type;
        struct Sema_IdentPtr tmplt;
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
                        struct Parser_Allocators *allocs,
                        struct DiagVec *diags);
