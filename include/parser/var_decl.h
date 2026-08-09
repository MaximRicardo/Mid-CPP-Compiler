#pragma once

#include "allocator.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/expr.h"
#include "sema/scope.h"
#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct midpar_VarDecl;

struct midpar_VarDeclInst {
    struct midpar_Type type;
    const char *name;

    // a var decl can have: an initializer, a ctor, or neither
    union {
        struct {
            struct midpar_Expr *expr;
            midlex_TokenIter start; // points to the first token of the
                                    // initialization expr if there is one
        } init;

        struct {
            struct midpar_ExprVec args;
            struct midpar_FuncDecl *ctor;
        } ctor;
    };
    bool has_ctor;

    bool typechecked;
};
midgen_dynarray_struct_named(midpar_VarDeclInstPVec,
                             struct midpar_VarDeclInst *);

void midpar_VarDeclInst_deinit(struct midpar_VarDeclInst *self);
void midpar_copy_var_decl_inst(struct midpar_VarDeclInst *dest,
                               const struct midpar_VarDeclInst *src,
                               struct midsema_Scope *dest_scope,
                               struct midpar_Allocators *allocs);

struct midpar_VarDecl {
    struct midpar_VarDeclInstPVec insts;
};
midgen_dynarray_struct_named(midpar_VarDeclVec, struct midpar_VarDecl);
midgen_dynarray_struct_named(midpar_VarDeclPVec, struct midpar_VarDecl *);

void midpar_VarDecl_deinit(struct midpar_VarDecl *self);
void midpar_copy_var_decl(struct midpar_VarDecl *dest,
                          const struct midpar_VarDecl *src,
                          struct midsema_Scope *dest_scope,
                          struct midpar_Allocators *allocs);

struct midpar_ParseVarDeclFlags {
    bool add_to_scope; // if true, the variable is added as an identifier in
                       // the passed scope.
    bool single_inst;  // if true, the declaration is parsed under the
                       // assumption that it only declares a single instance.
                       // this is the case for stuff like function parameters.
    bool skip_init;    // if true, the var initializer won't be parsed, but
                       // init_start will still be set to the first token of
                       // the initializer if there is one.
};

midlex_TokenIter midpar_parse_var_decl(
    struct midpar_VarDecl *self, midlex_TokenIter start,
    const enum midlex_TokenType *end_types, mid_isize n_end_types,
    struct midpar_ParseVarDeclFlags flags, struct midsema_Scope *scope,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags);
// node     - ignored if add_to_scope is false.
midlex_TokenIter midpar_parse_var_decl_inst(
    struct midpar_VarDeclInst *self, midlex_TokenIter start,
    const enum midlex_TokenType *end_types, mid_isize n_end_types,
    const struct midpar_Type *base, struct midsema_Scope *scope,
    struct midpar_ParseVarDeclFlags flags, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags);
midlex_TokenIter midpar_parse_var_decl_inst_list(
    midlex_TokenIter start, const enum midlex_TokenType *end_types,
    mid_isize n_end_types, const struct midpar_Type *base,
    struct midpar_VarDeclInstPVec *insts, struct midpar_VarDecl *decl,
    struct midsema_Scope *scope, struct midpar_ParseVarDeclFlags flags,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags);

// expr_prealloced        - if true, inst->init.expr is assumed to be
//                          preallocated to a valid ptr.
midlex_TokenIter midpar_parse_var_decl_inst_def(
    const enum midlex_TokenType *end_types, mid_isize n_end_types,
    struct midpar_VarDeclInst *inst, bool expr_prealloced,
    struct midsema_Scope *scope, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags);
void midpar_parse_var_decl_def(const enum midlex_TokenType *end_types,
                               mid_isize n_end_types,
                               struct midpar_VarDecl *decl,
                               bool exprs_prealloced,
                               struct midsema_Scope *scope,
                               struct midpar_Allocators *allocs,
                               struct mid_DiagVec *diags);

struct midpar_VarDeclInst *
midpar_decl_inst_of_name(const struct midpar_VarDecl *decl, const char *name);

#ifdef __cplusplus
}
#endif
