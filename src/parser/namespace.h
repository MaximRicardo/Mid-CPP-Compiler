#pragma once

#include "diag.h"
#include "ints.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/scope.h"

struct Parser_Namespace {
    struct Parser_ASTNodePVec childs;
    const char *name; // NULL for anonymous namespaces
    struct Sema_Scope *scope;
    i32 ident_idx;
};

void Parser_Namespace_deinit(struct Parser_Namespace *self);
struct Sema_Ident *Parser_namespace_ident(const struct Parser_Namespace *self);
isize_t Parser_parse_namespace(struct Parser_ASTNode *node,
                               struct Sema_Scope *scope,
                               const struct Lexer_Token *toks, isize_t start,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags);
