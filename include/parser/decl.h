#pragma once

#include "diag.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "sema/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

// looks ahead to check whether a declaration is supposed to be a function or
// variable declaration
// out_mvp - was most vexing parse used to disambiguate the declaration type
bool midpar_decl_is_func(midlex_TokenIter start, struct midsema_Scope *scope,
                         struct midpar_Allocators *allocs,
                         struct mid_DiagVec *diags, bool *out_mvp);

#ifdef __cplusplus
}
#endif
