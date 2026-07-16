#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "parser/type.h"

constexpr char Parser_ctor_name[] = "__constructor";
constexpr char Parser_dtor_name[] = "__destructor";

struct Parser_FuncQuals {
    bool is_const;
    bool is_volatile;
    bool lv_ref;
    bool rv_ref;
    bool is_final;
    bool is_override;

    bool is_delete;  // void f() = delete;
    bool is_default; // void f() = default;
};

struct Parser_FuncDecl {
    struct Parser_Type type;
    struct Parser_ASTNodePVec params;
    struct Parser_ASTNodePVec nodes;
    const char *name;
    struct Sema_Scope *param_scope;
    const struct Lexer_Token *def_start; // points to the left curly '{'
    i32 ident_idx; // index of the identifier holding the function overload
                   // in the parent scope. -1 if there is no identifier
    enum Parser_ExprType op_overload; // the operator that got overloaded
    bool is_op_overload;
    struct Parser_FuncQuals quals;
    bool has_def; // does this node hold the definition of the func?
    bool variadic;
    bool is_tor; // if true the func is either a ctor or a dtor
    bool is_dtor;
};

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self);
void Parser_copy_func_decl(struct Parser_ASTNode *dest,
                           const struct Parser_ASTNode *src,
                           struct Sema_Scope *dest_scope,
                           struct Parser_Allocators *allocs);
struct Sema_Scope *Parser_func_parent(const struct Parser_FuncDecl *func);
struct Sema_Ident *Parser_func_ident(const struct Parser_FuncDecl *func);
struct Parser_ASTNodePVec
Parser_parse_func_params(const struct Lexer_Token *toks, isize_t lparen,
                         isize_t *out_rparen, struct Parser_ASTNode *parent,
                         struct Sema_Scope *scope, bool add_to_scope,
                         bool *out_variadic, struct Parser_Allocators *allocs,
                         struct DiagVec *diags);
isize_t Parser_parse_func_quals(const struct Lexer_Token *toks, isize_t start,
                                struct Parser_FuncQuals *out_quals,
                                struct DiagVec *diags);
// skip_def -  if true, the func definition won't be parsed, but def_start will
//             still be set to the first token of the definition and has_def
//             will still be set to true if there is a definition. used by
//             classes, which parse declarations first then definitions
isize_t Parser_parse_func_decl(const struct Lexer_Token *toks, isize_t start,
                               struct Parser_ASTNode *node,
                               struct Sema_Scope *scope, bool skip_def,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags);
isize_t Parser_parse_tor(const struct Lexer_Token *toks, isize_t start,
                         struct Parser_ASTNode *node, struct Sema_Scope *scope,
                         bool skip_def, struct Parser_Allocators *allocs,
                         struct DiagVec *diags);

// returns the idx of the closing curly bracket
isize_t Parser_parse_func_body(const struct Lexer_Token *toks, isize_t lcurly,
                               struct Parser_ASTNode *node,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags);

bool Parser_func_is_method(const struct Parser_FuncDecl *self);
bool Parser_func_is_ctor(const struct Parser_FuncDecl *self);
// cnt_ctors    - do constructors also count?
bool Parser_func_takes_implicit_this(const struct Parser_FuncDecl *self,
                                     bool cnt_ctors);
struct Parser_Type
Parser_implicit_this_type(const struct Parser_FuncDecl *self);
bool Parser_func_is_main(const struct Parser_FuncDecl *self);
