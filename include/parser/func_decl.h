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

// represents a member intializer in a ctor
struct midpar_FuncMemberInit {
    const char *name;
    struct midpar_Expr *inits;
    mid_isize n_inits;
};
midgen_dynarray_struct_named(midpar_FuncMemberInitPVec,
                             struct midpar_FuncMemberInit *);

void midpar_FuncMemberInit_deinit(struct midpar_FuncMemberInit *self);
void midpar_copy_func_memb_init(struct midpar_FuncMemberInit *dest,
                                const struct midpar_FuncMemberInit *src);

struct midpar_FuncDecl {
    struct midpar_Type ret;
    struct midpar_VarDeclPVec params;
    struct midpar_ASTNodePVec nodes;
    struct midpar_FuncMemberInitPVec memb_inits; // only used by ctors
    const char *name;
    struct midsema_Scope *param_scope;
    midlex_TokenIter def_start; // points to the left curly '{'
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
midpar_parse_func_params(midlex_TokenIter lparen, midlex_TokenIter *out_rparen,
                         struct midpar_ASTNode *parent,
                         struct midsema_Scope *scope, bool add_to_scope,
                         bool *out_variadic, struct midpar_Allocators *allocs,
                         struct mid_DiagVec *diags);
// skip_def -  if true, the func definition won't be parsed, but def_start will
//             still be set to the first token of the definition and has_def
//             will still be set to true if there is a definition. used by
//             classes, which parse declarations first then definitions
midlex_TokenIter midpar_parse_func_decl(struct midpar_FuncDecl *self,
                                        midlex_TokenIter start,
                                        struct midsema_Scope *scope,
                                        bool skip_def,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags);
midlex_TokenIter midpar_parse_tor(struct midpar_FuncDecl *self,
                                  midlex_TokenIter start,
                                  struct midsema_Scope *scope, bool skip_def,
                                  struct midpar_Allocators *allocs,
                                  struct mid_DiagVec *diags);

// returns the idx of the closing curly bracket or semicolon marking the end
// of the function
const struct midlex_Token *
midpar_parse_func_body(struct midpar_FuncDecl *self, midlex_TokenIter start,
                       struct midpar_Allocators *allocs,
                       struct mid_DiagVec *diags);

#ifdef __cplusplus
}
#endif
