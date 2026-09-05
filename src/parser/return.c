#include "parser/return.h"
#include "generics/bumpalloc.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "sema/typecheck.h"

void midpar_copy_return(struct midpar_Return *dest,
                        const struct midpar_Return *src,
                        struct midpar_Allocators *allocs)
{
    if (src->expr) {
        midgen_bumpmalloc(&allocs->expr, &dest->expr);
        *dest->expr = midpar_copy_expr(src->expr);
    }
}

midlex_TokenIter midpar_parse_return(struct midpar_Return *self,
                                     midlex_TokenIter start,
                                     struct midsema_Scope *scope,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags)
{
    assert(start->type == MIDLEX_TOKENTYPE_RETURN);

    *self = (struct midpar_Return){};

    midlex_TokenIter end = start + 1;

    if (end->type != MIDLEX_TOKENTYPE_SEMICOLON) {
        midgen_bumpmalloc(&allocs->expr, &self->expr);
        *self->expr = midpar_parse_expr(start + 1, MIDPAR_DEFAULT_ENDTYPES,
                                        &end, scope, diags);
    }

    midsema_typecheck_return(self, scope, diags);

    return end;
}
