#include "return.h"
#include "end_types.h"
#include "generics/bumpalloc.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr.h"
#include "sema/type.h"

void MidParser_copy_return(struct MidParser_Return *dest,
                        const struct MidParser_Return *src,
                        struct MidParser_Allocators *allocs)
{
    if (src->expr) {
        MidGen_bumpmalloc(&allocs->expr, &dest->expr);
        *dest->expr = MidParser_copy_expr(src->expr);
    }
}

mid_isize MidParser_parse_return(struct MidParser_Return *self,
                            const struct MidLexer_Token *toks, mid_isize start,
                            struct MidSema_Scope *scope,
                            struct MidParser_Allocators *allocs,
                            struct MidDiag_DiagVec *diags)
{
    assert(toks[start].type == MIDLEXER_TOKENTYPE_RETURN);

    *self = (struct MidParser_Return){};

    mid_isize end;

    if (toks[start + 1].type == MIDLEXER_TOKENTYPE_SEMICOLON) {
        end = start + 1;
    } else {
        MidGen_bumpmalloc(&allocs->expr, &self->expr);
        *self->expr = MidParser_parse_expr(
            toks, start + 1, MIDPARSER_DEFAULT_ENDTYPES, &end, scope, diags);
    }

    MidSema_typecheck_return(self, scope, diags);

    return end;
}
