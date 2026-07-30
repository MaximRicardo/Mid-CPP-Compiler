#include "return.h"
#include "end_types.h"
#include "generics/bumpalloc.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/expr.h"
#include "sema/type.h"

void Parser_copy_return(struct Parser_Return *dest,
                        const struct Parser_Return *src,
                        struct Parser_Allocators *allocs)
{
    if (src->expr) {
        gen_bumpmalloc(&allocs->expr, &dest->expr);
        *dest->expr = Parser_copy_expr(src->expr);
    }
}

isize_t Parser_parse_return(struct Parser_Return *self,
                            const struct Lexer_Token *toks, isize_t start,
                            struct Sema_Scope *scope,
                            struct Parser_Allocators *allocs,
                            struct DiagVec *diags)
{
    assert(toks[start].type == LEXER_TOKENTYPE_RETURN);

    *self = (struct Parser_Return){};

    isize_t end;

    if (toks[start + 1].type == LEXER_TOKENTYPE_SEMICOLON) {
        end = start + 1;
    } else {
        gen_bumpmalloc(&allocs->expr, &self->expr);
        *self->expr = Parser_parse_expr(
            toks, start + 1, PARSER_DEFAULT_ENDTYPES, &end, scope, diags);
    }

    Sema_typecheck_return(self, scope, diags);

    return end;
}
