#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/expr.h"
#include "sema/scope.h"

struct midpar_Return {
    struct midpar_Expr *expr;
};

void midpar_copy_return(struct midpar_Return *dest,
                        const struct midpar_Return *src,
                        struct midpar_Allocators *allocs);
mid_isize midpar_parse_return(struct midpar_Return *self,
                              const struct midlex_Token *toks, mid_isize start,
                              struct midsema_Scope *scope,
                              struct midpar_Allocators *allocs,
                              struct mid_DiagVec *diags);
