#include "parser/return.h"
#include "generics/bumpalloc.h"
#include "ints.h"
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

mid_isize midpar_parse_return(struct midpar_Return *self,
                              const struct midlex_Token *toks, mid_isize start,
                              struct midsema_Scope *scope,
                              struct midpar_Allocators *allocs,
                              struct mid_DiagVec *diags)
{
    assert(toks[start].type == MIDLEX_TOKENTYPE_RETURN);

    *self = (struct midpar_Return){};

    mid_isize end;

    if (toks[start + 1].type == MIDLEX_TOKENTYPE_SEMICOLON) {
        end = start + 1;
    } else {
        midgen_bumpmalloc(&allocs->expr, &self->expr);
        *self->expr = midpar_parse_expr(
            toks, start + 1, MIDPAR_DEFAULT_ENDTYPES, &end, scope, diags);
    }

    midsema_typecheck_return(self, scope, diags);

    return end;
}
