#pragma once

#include "diag.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

struct Parser_Namespace {
    struct Sema_IdentPtr ident;
    struct Parser_ASTNodePVec childs;
    struct Sema_Scope *scope;
    const char *name; // NULL for anonymous namespaces
};

void Parser_Namespace_deinit(struct Parser_Namespace *self);
void Parser_copy_namespace(struct Parser_Namespace *dest,
                           const struct Parser_Namespace *src,
                           struct Sema_Scope *dest_scope,
                           struct Parser_Allocators *allocs);
isize_t Parser_parse_namespace(struct Parser_ASTNode *node,
                               struct Sema_Scope *scope,
                               const struct Lexer_Token *toks, isize_t start,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags);
