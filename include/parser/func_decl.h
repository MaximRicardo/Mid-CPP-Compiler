#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "parser/type.h"
#include "parser/var_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

constexpr char midpar_ctor_name[] = "$__constructor";
constexpr char midpar_dtor_name[] = "$__destructor";

struct midpar_FuncQuals {
    bool is_const;
    bool is_volatile;
    bool lv_ref;
    bool rv_ref;
    bool is_final;
    bool is_virtual;
    bool is_override;
    bool is_explicit;
    bool is_constexpr;

    bool is_delete;  // void f() = delete;
    bool is_default; // void f() = default;
};

struct midpar_FuncDecl {
    struct midpar_Type ret;
    struct midpar_VarDeclPVec params;
    struct midpar_ASTNodePVec nodes;
    const char *name;
    struct midsema_Scope *param_scope;
    const struct midlex_Token *def_start; // points to the left curly '{'
    int32_t ident_idx; // index of the identifier holding the function overload
                       // in the parent scope. -1 if there is no identifier
    enum midpar_ExprType op_overload; // the operator that got overloaded
    bool is_op_overload;
    struct midpar_FuncQuals quals;
    bool has_def; // does this node hold the definition of the func?
    bool variadic;
    bool is_tor; // if true the func is either a ctor or a dtor
    bool is_dtor;
};
midgen_dynarray_struct_named(midpar_FuncDeclPVec, struct midpar_FuncDecl *);

void midpar_FuncDecl_deinit(struct midpar_FuncDecl *self);
void midpar_copy_func_decl(struct midpar_FuncDecl *dest,
                           const struct midpar_FuncDecl *src,
                           struct midsema_Scope *dest_scope,
                           struct midpar_Allocators *allocs);
struct midsema_Scope *midpar_func_parent(const struct midpar_FuncDecl *func);
struct midsema_Ident *midpar_func_ident(const struct midpar_FuncDecl *func);
struct midpar_VarDeclPVec
midpar_parse_func_params(const struct midlex_Token *toks, mid_isize lparen,
                         mid_isize *out_rparen, struct midpar_ASTNode *parent,
                         struct midsema_Scope *scope, bool add_to_scope,
                         bool *out_variadic, struct midpar_Allocators *allocs,
                         struct mid_DiagVec *diags);
mid_isize midpar_parse_func_quals(const struct midlex_Token *toks,
                                  mid_isize start,
                                  struct midpar_FuncQuals *out_quals,
                                  bool is_constexpr, struct mid_DiagVec *diags);
// skip_def -  if true, the func definition won't be parsed, but def_start will
//             still be set to the first token of the definition and has_def
//             will still be set to true if there is a definition. used by
//             classes, which parse declarations first then definitions
mid_isize midpar_parse_func_decl(struct midpar_FuncDecl *self,
                                 const struct midlex_Token *toks,
                                 mid_isize start, struct midsema_Scope *scope,
                                 bool skip_def,
                                 struct midpar_Allocators *allocs,
                                 struct mid_DiagVec *diags);
mid_isize midpar_parse_tor(struct midpar_FuncDecl *self,
                           const struct midlex_Token *toks, mid_isize start,
                           struct midsema_Scope *scope, bool skip_def,
                           struct midpar_Allocators *allocs,
                           struct mid_DiagVec *diags);

// returns the idx of the closing curly bracket
mid_isize midpar_parse_func_body(struct midpar_FuncDecl *self,
                                 const struct midlex_Token *toks,
                                 mid_isize lcurly,
                                 struct midpar_Allocators *allocs,
                                 struct mid_DiagVec *diags);

bool midpar_func_is_method(const struct midpar_FuncDecl *self);
bool midpar_func_is_ctor(const struct midpar_FuncDecl *self);
bool midpar_func_is_default_ctor(const struct midpar_FuncDecl *self);
bool midpar_func_is_copy_ctor(const struct midpar_FuncDecl *self);
bool midpar_func_is_move_ctor(const struct midpar_FuncDecl *self);
// cnt_ctors    - do constructors also count?
bool midpar_func_takes_implicit_this(const struct midpar_FuncDecl *self,
                                     bool cnt_ctors);
struct midpar_Type
midpar_implicit_this_type(const struct midpar_FuncDecl *self);
bool midpar_func_is_main(const struct midpar_FuncDecl *self);
bool midpar_is_user_provided(const struct midpar_FuncDecl *self);

#ifdef __cplusplus
}
#endif
