#pragma once

#include "ints.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "sema/scope.h"

// this_quals - set to NULL to if "this" is explicitly passed
bool MidSema_is_func_viable(const struct MidParser_Expr *args, mid_isize n_args,
                            const struct MidParser_FuncDecl *func,
                            const struct MidParser_TypeDataQual *this_quals);
// this_quals - set to NULL to if "this" is explicitly passed
struct MidParser_FuncDeclPVec
MidSema_viable_funcs(const struct MidParser_Expr *args, mid_isize n_args,
                     const struct MidParser_FuncDeclPVec *funcs,
                     const struct MidParser_TypeDataQual *this_quals);
// this_quals - set to NULL to if "this" is explicitly passed
struct MidParser_FuncDecl *
MidSema_best_viable_func(const struct MidParser_Expr *args, mid_isize n_args,
                         const struct MidParser_FuncDeclPVec *funcs,
                         const struct MidParser_TypeDataQual *this_quals);

struct MidParser_FuncDeclPVec MidSema_find_candidate_funcs(
    const char *name, const struct MidParser_Expr *args, mid_isize n_args,
    struct MidSema_Scope *scope, bool is_qualified);
struct MidParser_FuncDecl *MidSema_find_func(const char *name,
                                             const struct MidParser_Expr *args,
                                             mid_isize n_args,
                                             struct MidSema_Scope *scope,
                                             bool is_qualified);
struct MidParser_FuncDeclPVec
MidSema_find_candidate_methods(const char *name, struct MidSema_Scope *scope);
struct MidParser_FuncDecl *
MidSema_find_method(const char *name, const struct MidParser_Expr *args,
                    mid_isize n_args, struct MidSema_Scope *scope,
                    const struct MidParser_TypeDataQual *this_quals);
struct MidParser_FuncDecl *
MidSema_find_op_overload(enum MidParser_ExprType op,
                         const struct MidParser_Expr *args, mid_isize n_args,
                         struct MidSema_Scope *scope);
