#pragma once

#include "diag.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/expr.h"
#include "sema/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

struct midpar_Return {
    struct midpar_Expr *expr;
};

void midpar_copy_return(struct midpar_Return *dest,
                        const struct midpar_Return *src,
                        struct midpar_Allocators *allocs);
midlex_TokenIter midpar_parse_return(struct midpar_Return *self,
                                     midlex_TokenIter start,
                                     struct midsema_Scope *scope,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags);

#ifdef __cplusplus
}
#endif
