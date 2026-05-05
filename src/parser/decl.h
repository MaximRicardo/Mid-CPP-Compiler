#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "sema/scope.h"

// looks ahead to check whether a declaration is supposed to be a function or
// variable declaration
// out_mvp - was most vexing parse used to disambiguate
bool Parser_decl_is_func(const struct Lexer_Token *toks, isize_t start,
                         struct Sema_Scope *scope,
                         struct Parser_Allocators *allocs,
                         struct DiagVec *diags, bool *out_mvp);
