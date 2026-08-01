#pragma once

#include "diag.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

struct midpar_Namespace {
    struct midsema_IdentPtr ident;
    struct midpar_ASTNodePVec childs;
    struct midsema_Scope *scope;
    const char *name; // NULL for anonymous namespaces
};

void midpar_Namespace_deinit(struct midpar_Namespace *self);
void midpar_copy_namespace(struct midpar_Namespace *dest,
                           const struct midpar_Namespace *src,
                           struct midsema_Scope *dest_scope,
                           struct midpar_Allocators *allocs);
mid_isize midpar_parse_namespace(struct midpar_Namespace *self,
                                 struct midsema_Scope *scope,
                                 const struct midlex_Token *toks,
                                 mid_isize start,
                                 struct midpar_Allocators *allocs,
                                 struct mid_DiagVec *diags);
