#pragma once

#include "ints.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "sema/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

// this_quals - set to NULL to if "this" is explicitly passed
bool midsema_is_func_viable(const struct midpar_Expr *args, mid_isize n_args,
                            const struct midpar_FuncDecl *func,
                            const struct midpar_TypeDataQual *this_quals);
// this_quals - set to NULL to if "this" is explicitly passed
struct midpar_FuncDeclPVec
midsema_viable_funcs(const struct midpar_Expr *args, mid_isize n_args,
                     const struct midpar_FuncDeclPVec *funcs,
                     const struct midpar_TypeDataQual *this_quals);
// this_quals - set to NULL to if "this" is explicitly passed
struct midpar_FuncDecl *
midsema_best_viable_func(const struct midpar_Expr *args, mid_isize n_args,
                         const struct midpar_FuncDeclPVec *funcs,
                         const struct midpar_TypeDataQual *this_quals);

struct midpar_FuncDeclPVec
midsema_find_candidate_funcs(const char *name, const struct midpar_Expr *args,
                             mid_isize n_args, struct midsema_Scope *scope,
                             bool is_qualified);
struct midpar_FuncDecl *midsema_find_func(const char *name,
                                          const struct midpar_Expr *args,
                                          mid_isize n_args,
                                          struct midsema_Scope *scope,
                                          bool is_qualified);
struct midpar_FuncDeclPVec
midsema_find_candidate_methods(const char *name, struct midsema_Scope *scope);
struct midpar_FuncDecl *
midsema_find_method(const char *name, const struct midpar_Expr *args,
                    mid_isize n_args, struct midsema_Scope *scope,
                    const struct midpar_TypeDataQual *this_quals);
struct midpar_FuncDecl *midsema_find_op_overload(enum midpar_ExprType op,
                                                 const struct midpar_Expr *args,
                                                 mid_isize n_args,
                                                 struct midsema_Scope *scope);

#ifdef __cplusplus
}
#endif
