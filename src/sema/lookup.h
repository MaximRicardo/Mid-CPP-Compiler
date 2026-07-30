#pragma once

#include "ints.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "sema/scope.h"

// this_quals - set to NULL to if "this" is explicitly passed
bool Sema_is_func_viable(const struct Parser_Expr *args, isize_t n_args,
                         const struct Parser_FuncDecl *func,
                         const struct Parser_TypeDataQual *this_quals);
// this_quals - set to NULL to if "this" is explicitly passed
struct Parser_FuncDeclPVec
Sema_viable_funcs(const struct Parser_Expr *args, isize_t n_args,
                  const struct Parser_FuncDeclPVec *funcs,
                  const struct Parser_TypeDataQual *this_quals);
// this_quals - set to NULL to if "this" is explicitly passed
struct Parser_FuncDecl *
Sema_best_viable_func(const struct Parser_Expr *args, isize_t n_args,
                      const struct Parser_FuncDeclPVec *funcs,
                      const struct Parser_TypeDataQual *this_quals);

struct Parser_FuncDeclPVec
Sema_find_candidate_funcs(const char *name, const struct Parser_Expr *args,
                          isize_t n_args, struct Sema_Scope *scope,
                          bool is_qualified);
struct Parser_FuncDecl *Sema_find_func(const char *name,
                                       const struct Parser_Expr *args,
                                       isize_t n_args, struct Sema_Scope *scope,
                                       bool is_qualified);
struct Parser_FuncDeclPVec
Sema_find_candidate_methods(const char *name, struct Sema_Scope *scope);
struct Parser_FuncDecl *
Sema_find_method(const char *name, const struct Parser_Expr *args,
                 isize_t n_args, struct Sema_Scope *scope,
                 const struct Parser_TypeDataQual *this_quals);
struct Parser_FuncDecl *Sema_find_op_overload(enum Parser_ExprType op,
                                              const struct Parser_Expr *args,
                                              isize_t n_args,
                                              struct Sema_Scope *scope);
