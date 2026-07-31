#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/expr.h"
#include "sema/scope.h"

struct MidParser_Return {
    struct MidParser_Expr *expr;
};

void MidParser_copy_return(struct MidParser_Return *dest,
                        const struct MidParser_Return *src,
                        struct MidParser_Allocators *allocs);
mid_isize MidParser_parse_return(struct MidParser_Return *self,
                            const struct MidLexer_Token *toks, mid_isize start,
                            struct MidSema_Scope *scope,
                            struct MidParser_Allocators *allocs,
                            struct MidDiag_DiagVec *diags);
