#pragma once

#include "ints.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "sema/scope.h"

bool Sema_is_func_viable(const struct Parser_Expr *args, isize_t n_args,
                         const struct Parser_FuncDecl *func);
struct Parser_ASTNodePVec
Sema_viable_funcs(const struct Parser_Expr *args, isize_t n_args,
                  const struct Parser_ASTNodePVec *funcs);
struct Parser_ASTNode *
Sema_best_viable_func(const struct Parser_Expr *args, isize_t n_args,
                      const struct Parser_ASTNodePVec *funcs);
// accounts for argument dependent lookup
struct Parser_ASTNode *Sema_find_func_adl(const char *name,
                                          const struct Parser_Expr *args,
                                          isize_t n_args,
                                          struct Sema_Scope *scope);
struct Parser_ASTNode *Sema_find_op_overload(enum Parser_ExprType op,
                                             const struct Parser_Expr *args,
                                             isize_t n_args,
                                             struct Sema_Scope *scope);
