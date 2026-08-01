#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "sema/scope.h"

// looks ahead to check whether a declaration is supposed to be a function or
// variable declaration
// out_mvp - was most vexing parse used to disambiguate
bool midpar_decl_is_func(const struct midlex_Token *toks, mid_isize start,
                         struct midsema_Scope *scope,
                         struct midpar_Allocators *allocs,
                         struct mid_DiagVec *diags, bool *out_mvp);
