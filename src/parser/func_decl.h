#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "parser/type.h"
#include "parser/var_decl.h"

constexpr char MidParser_ctor_name[] = "__constructor";
constexpr char MidParser_dtor_name[] = "__destructor";

struct MidParser_FuncQuals {
    bool is_const;
    bool is_volatile;
    bool lv_ref;
    bool rv_ref;
    bool is_final;
    bool is_override;

    bool is_delete;  // void f() = delete;
    bool is_default; // void f() = default;
};

struct MidParser_FuncDecl {
    struct MidParser_Type ret;
    struct MidParser_VarDeclPVec params;
    struct MidParser_ASTNodePVec nodes;
    const char *name;
    struct MidSema_Scope *param_scope;
    const struct MidLexer_Token *def_start; // points to the left curly '{'
    i32 ident_idx; // index of the identifier holding the function overload
                   // in the parent scope. -1 if there is no identifier
    enum MidParser_ExprType op_overload; // the operator that got overloaded
    bool is_op_overload;
    struct MidParser_FuncQuals quals;
    bool has_def; // does this node hold the definition of the func?
    bool variadic;
    bool is_tor; // if true the func is either a ctor or a dtor
    bool is_dtor;
};
MidGen_dynarray_struct_named(MidParser_FuncDeclPVec, struct MidParser_FuncDecl *);

void MidParser_FuncDecl_deinit(struct MidParser_FuncDecl *self);
void MidParser_copy_func_decl(struct MidParser_FuncDecl *dest,
                           const struct MidParser_FuncDecl *src,
                           struct MidSema_Scope *dest_scope,
                           struct MidParser_Allocators *allocs);
struct MidSema_Scope *MidParser_func_parent(const struct MidParser_FuncDecl *func);
struct MidSema_Ident *MidParser_func_ident(const struct MidParser_FuncDecl *func);
struct MidParser_VarDeclPVec
MidParser_parse_func_params(const struct MidLexer_Token *toks, mid_isize lparen,
                         mid_isize *out_rparen, struct MidParser_ASTNode *parent,
                         struct MidSema_Scope *scope, bool add_to_scope,
                         bool *out_variadic, struct MidParser_Allocators *allocs,
                         struct MidDiag_DiagVec *diags);
mid_isize MidParser_parse_func_quals(const struct MidLexer_Token *toks, mid_isize start,
                                struct MidParser_FuncQuals *out_quals,
                                struct MidDiag_DiagVec *diags);
// skip_def -  if true, the func definition won't be parsed, but def_start will
//             still be set to the first token of the definition and has_def
//             will still be set to true if there is a definition. used by
//             classes, which parse declarations first then definitions
mid_isize MidParser_parse_func_decl(struct MidParser_FuncDecl *self,
                               const struct MidLexer_Token *toks, mid_isize start,
                               struct MidSema_Scope *scope, bool skip_def,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags);
mid_isize MidParser_parse_tor(struct MidParser_FuncDecl *self,
                         const struct MidLexer_Token *toks, mid_isize start,
                         struct MidSema_Scope *scope, bool skip_def,
                         struct MidParser_Allocators *allocs,
                         struct MidDiag_DiagVec *diags);

// returns the idx of the closing curly bracket
mid_isize MidParser_parse_func_body(struct MidParser_FuncDecl *self,
                               const struct MidLexer_Token *toks, mid_isize lcurly,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags);

bool MidParser_func_is_method(const struct MidParser_FuncDecl *self);
bool MidParser_func_is_ctor(const struct MidParser_FuncDecl *self);
// cnt_ctors    - do constructors also count?
bool MidParser_func_takes_implicit_this(const struct MidParser_FuncDecl *self,
                                     bool cnt_ctors);
struct MidParser_Type
MidParser_implicit_this_type(const struct MidParser_FuncDecl *self);
bool MidParser_func_is_main(const struct MidParser_FuncDecl *self);
