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

struct midpar_TmpltParam;
midgen_dynarray_struct_named(midpar_TmpltParamPVec, struct midpar_TmpltParam *);

enum midpar_TmpltArgType {
    MIDPAR_TMPLTARG_NONTYPE,
    MIDPAR_TMPLTARG_TYPE,
    MIDPAR_TMPLTARG_TMPLT,
};

struct midpar_TmpltArg {
    union {
        struct midpar_Expr non_type;
        struct midpar_Type type;
        struct midsema_IdentPtr tmplt;
    };
    enum midpar_TmpltArgType kind;
};
midgen_dynarray_struct_named(midpar_TmpltArgVec, struct midpar_TmpltArg);

void midpar_TmpltArg_deinit(struct midpar_TmpltArg *self);
struct midpar_TmpltArg midpar_copy_tmplt_arg(struct midpar_TmpltArg *src);
struct midpar_TmpltArgVec
midpar_copy_tmplt_argvec(const struct midpar_TmpltArgVec *src);

struct midpar_TmpltInst {
    struct midpar_TmpltArgVec args;
    struct midpar_ASTNode *inst;
    struct midsema_Scope *scope; // the inst node's parent scope, which is a
                                 // child of the template scope
};
midgen_dynarray_struct_named(midpar_TmpltInstVec, struct midpar_TmpltInst);

void midpar_TmpltInst_deinit(struct midpar_TmpltInst *self);

struct midpar_Tmplt {
    struct midpar_TmpltInstVec insts;
    struct midpar_TmpltParamPVec params;
    struct midpar_ASTNode *child;
    struct midsema_Scope *scope;
};

void midpar_Tmplt_deinit(struct midpar_Tmplt *self);
void midpar_copy_tmplt(struct midpar_Tmplt *dest,
                       const struct midpar_Tmplt *src,
                       struct midsema_Scope *dest_scope,
                       struct midpar_Allocators *allocs);

enum midpar_TmpltParamType {
    MIDPAR_TMPLTPARAM_NONTYPE,
    MIDPAR_TMPLTPARAM_TYPE,
    MIDPAR_TMPLTPARAM_TMPLT,
};

struct midpar_TmpltNonTypeParam {
    struct midpar_Type type;
    const char *name;
    struct midpar_Expr *def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void midpar_TmpltNonTypeParam_deinit(struct midpar_TmpltNonTypeParam *self);

struct midpar_TmpltTypeParam {
    const char *name;
    struct midpar_Type *def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void midpar_TmpltTypeParam_deinit(struct midpar_TmpltTypeParam *self);

struct midpar_TmpltTmpltParam {
    struct midpar_Tmplt *tmplt;
    const char *name;
    struct midsema_IdentPtr def_arg;
    i32 ident_idx; // idx of the identifier in the tmplt scope
    bool variadic;
};

void midpar_TmpltTmpltParam_deinit(struct midpar_TmpltTmpltParam *self);

struct midpar_TmpltParam {
    // NOTE: THIS UNION NEEDS TO GO FIRST TO ALLOW POINTER CONVERSIONS
    union {
        struct midpar_TmpltNonTypeParam non_type;
        struct midpar_TmpltTypeParam type;
        struct midpar_TmpltTmpltParam tmplt;
    };
    enum midpar_TmpltParamType kind;
};

void midpar_TmpltParam_deinit(struct midpar_TmpltParam *self);
void midpar_copy_tmplt_param(struct midpar_TmpltParam *dest,
                             const struct midpar_TmpltParam *src,
                             struct midpar_Allocators *allocs);

mid_isize midpar_parse_tmplt(struct midpar_Tmplt *self,
                             struct midsema_Scope *scope,
                             const struct midlex_Token *toks, mid_isize start,
                             struct midpar_Allocators *allocs,
                             struct mid_DiagVec *diags);
struct midsema_Ident *midpar_tmplt_ident(const struct midpar_Tmplt *self);
struct midpar_TmpltArgVec
midpar_parse_tmplt_args(const struct midlex_Token *toks, mid_isize l_angle,
                        mid_isize *out_r_angle, struct midsema_Scope *scope,
                        struct midpar_Allocators *allocs,
                        struct mid_DiagVec *diags);

mid_isize midpar_tmplt_param_idx(const struct midpar_Tmplt *tmplt,
                                 const char *name);
