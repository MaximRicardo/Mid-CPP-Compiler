#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/expr.h"
#include "sema/scope.h"

struct Parser_Return {
    struct Parser_Expr *expr;
};

isize_t Parser_parse_return(const struct Lexer_Token *toks, isize_t start,
                            struct Parser_Return *ret,
                            struct Parser_ASTNode *node,
                            struct Sema_Scope *scope,
                            struct Parser_Allocators *allocs,
                            struct DiagVec *diags);
