#pragma once

#include "diag.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

struct MidParser_Namespace {
    struct MidSema_IdentPtr ident;
    struct MidParser_ASTNodePVec childs;
    struct MidSema_Scope *scope;
    const char *name; // NULL for anonymous namespaces
};

void MidParser_Namespace_deinit(struct MidParser_Namespace *self);
void MidParser_copy_namespace(struct MidParser_Namespace *dest,
                           const struct MidParser_Namespace *src,
                           struct MidSema_Scope *dest_scope,
                           struct MidParser_Allocators *allocs);
mid_isize MidParser_parse_namespace(struct MidParser_Namespace *self,
                               struct MidSema_Scope *scope,
                               const struct MidLexer_Token *toks, mid_isize start,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags);
