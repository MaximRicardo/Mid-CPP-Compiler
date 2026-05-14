#pragma once

#include "ints.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "sema/scope.h"

bool Sema_is_func_viable(const struct Parser_Expr *args, isize_t n_args,
                         const struct Parser_FuncDecl *func, bool this_passed);
struct Parser_ASTNodePVec
Sema_viable_funcs(const struct Parser_Expr *args, isize_t n_args,
                  const struct Parser_ASTNodePVec *funcs, bool this_passed);
struct Parser_ASTNode *
Sema_best_viable_func(const struct Parser_Expr *args, isize_t n_args,
                      const struct Parser_ASTNodePVec *funcs, bool this_passed);
struct Parser_ASTNode *Sema_find_func_adl(const char *name,
                                          const struct Parser_Expr *args,
                                          isize_t n_args, bool this_passed,
                                          struct Sema_Scope *scope,
                                          bool do_adl);
struct Parser_ASTNode *Sema_find_op_overload(enum Parser_ExprType op,
                                             const struct Parser_Expr *args,
                                             isize_t n_args,
                                             struct Sema_Scope *scope);
