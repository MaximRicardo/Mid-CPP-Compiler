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

struct Parser_VarDecl;

struct Parser_VarDeclInst {
    struct Parser_Type type;
    const char *name;

    // a var decl can have: an initializer, a ctor, or neither
    union {
        struct {
            struct Parser_Expr *expr;
            const struct Lexer_Token
                *start; // points to the first token of the
                        // initialization expr if there is one
        } init;

        struct {
            struct Parser_ExprVec args;
            struct Parser_FuncDecl *ctor;
        } ctor;
    };
    bool has_ctor;

    bool typechecked;
};
gen_dynarray_struct_named(Parser_VarDeclInstPVec, struct Parser_VarDeclInst *);

void Parser_VarDeclInst_deinit(struct Parser_VarDeclInst *self);
void Parser_copy_var_decl_inst(struct Parser_VarDeclInst *dest,
                               const struct Parser_VarDeclInst *src,
                               struct Sema_Scope *dest_scope,
                               struct Parser_Allocators *allocs);

struct Parser_VarDecl {
    struct Parser_VarDeclInstPVec insts;
};
gen_dynarray_struct_named(Parser_VarDeclVec, struct Parser_VarDecl);
gen_dynarray_struct_named(Parser_VarDeclPVec, struct Parser_VarDecl *);

void Parser_VarDecl_deinit(struct Parser_VarDecl *self);
void Parser_copy_var_decl(struct Parser_VarDecl *dest,
                          const struct Parser_VarDecl *src,
                          struct Sema_Scope *dest_scope,
                          struct Parser_Allocators *allocs);

struct Parser_ParseVarDeclFlags {
    bool add_to_scope; // if true, the variable is added as an identifier in
                       // the passed scope.
    bool single_inst;  // if true, the declaration is parsed under the
                       // assumption that it only declares a single instance.
                       // this is the case for stuff like function parameters.
    bool skip_init;    // if true, the var initializer won't be parsed, but
                       // init_start will still be set to the first token of
                       // the initializer if there is one.
};

isize_t Parser_parse_var_decl(
    struct Parser_VarDecl *self, const struct Lexer_Token *toks, isize_t start,
    const enum Lexer_TokenType *end_types, isize_t n_end_types,
    struct Parser_ParseVarDeclFlags flags, struct Sema_Scope *scope,
    struct Parser_Allocators *allocs, struct DiagVec *diags);
// node     - ignored if add_to_scope is false.
isize_t Parser_parse_var_decl_inst(
    struct Parser_VarDeclInst *self, const struct Lexer_Token *toks,
    isize_t start, const enum Lexer_TokenType *end_types, isize_t n_end_types,
    const struct Parser_Type *base, struct Sema_Scope *scope,
    struct Parser_ParseVarDeclFlags flags, struct Parser_Allocators *allocs,
    struct DiagVec *diags);
isize_t Parser_parse_var_decl_inst_list(
    const struct Lexer_Token *toks, isize_t start,
    const enum Lexer_TokenType *end_types, isize_t n_end_types,
    const struct Parser_Type *base, struct Parser_VarDeclInstPVec *insts,
    struct Parser_VarDecl *decl, struct Sema_Scope *scope,
    struct Parser_ParseVarDeclFlags flags, struct Parser_Allocators *allocs,
    struct DiagVec *diags);

// expr_prealloced        - if true, inst->init.expr is assumed to be
//                          preallocated to a valid ptr.
isize_t Parser_parse_var_decl_inst_def(
    const struct Lexer_Token *toks, const enum Lexer_TokenType *end_types,
    isize_t n_end_types, struct Parser_VarDeclInst *inst, bool expr_prealloced,
    struct Sema_Scope *scope, struct Parser_Allocators *allocs,
    struct DiagVec *diags);
void Parser_parse_var_decl_def(const struct Lexer_Token *toks,
                               const enum Lexer_TokenType *end_types,
                               isize_t n_end_types, struct Parser_VarDecl *decl,
                               bool exprs_prealloced, struct Sema_Scope *scope,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags);

struct Parser_VarDeclInst *
Parser_decl_inst_of_name(const struct Parser_VarDecl *decl, const char *name);
