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

struct MidParser_TmpltParam;
MidGen_dynarray_struct_named(MidParser_TmpltParamPVec,
                             struct MidParser_TmpltParam *);

enum MidParser_TmpltArgType {
    MIDPARSER_TMPLTARG_NONTYPE,
    MIDPARSER_TMPLTARG_TYPE,
    MIDPARSER_TMPLTARG_TMPLT,
};

struct MidParser_TmpltArg {
    union {
        struct MidParser_Expr non_type;
        struct MidParser_Type type;
        struct MidSema_IdentPtr tmplt;
    };
    enum MidParser_TmpltArgType kind;
};
MidGen_dynarray_struct_named(MidParser_TmpltArgVec, struct MidParser_TmpltArg);

void MidParser_TmpltArg_deinit(struct MidParser_TmpltArg *self);
struct MidParser_TmpltArg
MidParser_copy_tmplt_arg(struct MidParser_TmpltArg *src);
struct MidParser_TmpltArgVec
MidParser_copy_tmplt_argvec(const struct MidParser_TmpltArgVec *src);

struct MidParser_TmpltInst {
    struct MidParser_TmpltArgVec args;
    struct MidParser_ASTNode *inst;
    struct MidSema_Scope *scope; // the inst node's parent scope, which is a
                                 // child of the template scope
};
MidGen_dynarray_struct_named(MidParser_TmpltInstVec,
                             struct MidParser_TmpltInst);

void MidParser_TmpltInst_deinit(struct MidParser_TmpltInst *self);

struct MidParser_Tmplt {
    struct MidParser_TmpltInstVec insts;
    struct MidParser_TmpltParamPVec params;
    struct MidParser_ASTNode *child;
    struct MidSema_Scope *scope;
};

void MidParser_Tmplt_deinit(struct MidParser_Tmplt *self);
void MidParser_copy_tmplt(struct MidParser_Tmplt *dest,
                          const struct MidParser_Tmplt *src,
                          struct MidSema_Scope *dest_scope,
                          struct MidParser_Allocators *allocs);

enum MidParser_TmpltParamType {
    MIDPARSER_TMPLTPARAM_NONTYPE,
    MIDPARSER_TMPLTPARAM_TYPE,
    MIDPARSER_TMPLTPARAM_TMPLT,
};

struct MidParser_TmpltNonTypeParam {
    struct MidParser_Type type;
    const char *name;
    struct MidParser_Expr *def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void MidParser_TmpltNonTypeParam_deinit(
    struct MidParser_TmpltNonTypeParam *self);

struct MidParser_TmpltTypeParam {
    const char *name;
    struct MidParser_Type *def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void MidParser_TmpltTypeParam_deinit(struct MidParser_TmpltTypeParam *self);

struct MidParser_TmpltTmpltParam {
    struct MidParser_Tmplt *tmplt;
    const char *name;
    struct MidSema_IdentPtr def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void MidParser_TmpltTmpltParam_deinit(struct MidParser_TmpltTmpltParam *self);

struct MidParser_TmpltParam {
    // NOTE: THIS UNION NEEDS TO GO FIRST TO ALLOW POINTER CONVERSIONS
    union {
        struct MidParser_TmpltNonTypeParam non_type;
        struct MidParser_TmpltTypeParam type;
        struct MidParser_TmpltTmpltParam tmplt;
    };
    enum MidParser_TmpltParamType kind;
};

void MidParser_TmpltParam_deinit(struct MidParser_TmpltParam *self);
void MidParser_copy_tmplt_param(struct MidParser_TmpltParam *dest,
                                const struct MidParser_TmpltParam *src,
                                struct MidParser_Allocators *allocs);

mid_isize MidParser_parse_tmplt(struct MidParser_Tmplt *self,
                                struct MidSema_Scope *scope,
                                const struct MidLexer_Token *toks,
                                mid_isize start,
                                struct MidParser_Allocators *allocs,
                                struct MidDiag_DiagVec *diags);
struct MidSema_Ident *MidParser_tmplt_ident(const struct MidParser_Tmplt *self);
struct MidParser_TmpltArgVec
MidParser_parse_tmplt_args(const struct MidLexer_Token *toks, mid_isize l_angle,
                           mid_isize *out_r_angle, struct MidSema_Scope *scope,
                           struct MidParser_Allocators *allocs,
                           struct MidDiag_DiagVec *diags);

mid_isize MidParser_tmplt_param_idx(const struct MidParser_Tmplt *tmplt,
                                    const char *name);
