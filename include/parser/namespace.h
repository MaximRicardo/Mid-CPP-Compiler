#pragma once

#include "diag.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

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
midlex_TokenIter midpar_parse_namespace(struct midpar_Namespace *self,
                                        struct midsema_Scope *scope,
                                        midlex_TokenIter start,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags);

#ifdef __cplusplus
}
#endif
