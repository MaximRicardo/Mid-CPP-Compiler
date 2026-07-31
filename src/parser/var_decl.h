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

struct MidParser_VarDecl;

struct MidParser_VarDeclInst {
    struct MidParser_Type type;
    const char *name;

    // a var decl can have: an initializer, a ctor, or neither
    union {
        struct {
            struct MidParser_Expr *expr;
            const struct MidLexer_Token
                *start; // points to the first token of the
                        // initialization expr if there is one
        } init;

        struct {
            struct MidParser_ExprVec args;
            struct MidParser_FuncDecl *ctor;
        } ctor;
    };
    bool has_ctor;

    bool typechecked;
};
MidGen_dynarray_struct_named(MidParser_VarDeclInstPVec, struct MidParser_VarDeclInst *);

void MidParser_VarDeclInst_deinit(struct MidParser_VarDeclInst *self);
void MidParser_copy_var_decl_inst(struct MidParser_VarDeclInst *dest,
                               const struct MidParser_VarDeclInst *src,
                               struct MidSema_Scope *dest_scope,
                               struct MidParser_Allocators *allocs);

struct MidParser_VarDecl {
    struct MidParser_VarDeclInstPVec insts;
};
MidGen_dynarray_struct_named(MidParser_VarDeclVec, struct MidParser_VarDecl);
MidGen_dynarray_struct_named(MidParser_VarDeclPVec, struct MidParser_VarDecl *);

void MidParser_VarDecl_deinit(struct MidParser_VarDecl *self);
void MidParser_copy_var_decl(struct MidParser_VarDecl *dest,
                          const struct MidParser_VarDecl *src,
                          struct MidSema_Scope *dest_scope,
                          struct MidParser_Allocators *allocs);

struct MidParser_ParseVarDeclFlags {
    bool add_to_scope; // if true, the variable is added as an identifier in
                       // the passed scope.
    bool single_inst;  // if true, the declaration is parsed under the
                       // assumption that it only declares a single instance.
                       // this is the case for stuff like function parameters.
    bool skip_init;    // if true, the var initializer won't be parsed, but
                       // init_start will still be set to the first token of
                       // the initializer if there is one.
};

mid_isize MidParser_parse_var_decl(
    struct MidParser_VarDecl *self, const struct MidLexer_Token *toks, mid_isize start,
    const enum MidLexer_TokenType *end_types, mid_isize n_end_types,
    struct MidParser_ParseVarDeclFlags flags, struct MidSema_Scope *scope,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags);
// node     - ignored if add_to_scope is false.
mid_isize MidParser_parse_var_decl_inst(
    struct MidParser_VarDeclInst *self, const struct MidLexer_Token *toks,
    mid_isize start, const enum MidLexer_TokenType *end_types, mid_isize n_end_types,
    const struct MidParser_Type *base, struct MidSema_Scope *scope,
    struct MidParser_ParseVarDeclFlags flags, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags);
mid_isize MidParser_parse_var_decl_inst_list(
    const struct MidLexer_Token *toks, mid_isize start,
    const enum MidLexer_TokenType *end_types, mid_isize n_end_types,
    const struct MidParser_Type *base, struct MidParser_VarDeclInstPVec *insts,
    struct MidParser_VarDecl *decl, struct MidSema_Scope *scope,
    struct MidParser_ParseVarDeclFlags flags, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags);

// expr_prealloced        - if true, inst->init.expr is assumed to be
//                          preallocated to a valid ptr.
mid_isize MidParser_parse_var_decl_inst_def(
    const struct MidLexer_Token *toks, const enum MidLexer_TokenType *end_types,
    mid_isize n_end_types, struct MidParser_VarDeclInst *inst, bool expr_prealloced,
    struct MidSema_Scope *scope, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags);
void MidParser_parse_var_decl_def(const struct MidLexer_Token *toks,
                               const enum MidLexer_TokenType *end_types,
                               mid_isize n_end_types, struct MidParser_VarDecl *decl,
                               bool exprs_prealloced, struct MidSema_Scope *scope,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags);

struct MidParser_VarDeclInst *
MidParser_decl_inst_of_name(const struct MidParser_VarDecl *decl, const char *name);
