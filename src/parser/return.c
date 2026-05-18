#include "return.h"
#include "end_types.h"
#include "ints.h"
#include "lexer/token_type.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/type.h"

isize_t Parser_parse_return(const struct Lexer_Token *toks, isize_t start,
                            struct Parser_ASTNode *node,
                            struct Sema_Scope *scope,
                            struct Parser_Allocators *allocs,
                            struct DiagVec *diags)
{
    assert(toks[start].type == LEXER_TOKENTYPE_RETURN);

    auto self = &node->ret;
    *self = (struct Parser_Return){};

    isize_t end;

    if (toks[start + 1].type == LEXER_TOKENTYPE_SEMICOLON) {
        end = start + 1;
    } else {
        gen_bumpmalloc(&allocs->expr, &self->expr);
        *self->expr = Parser_parse_expr(
            toks, start + 1, PARSER_DEFAULT_ENDTYPES, &end, scope, diags);
    }

    Sema_typecheck_return(node, scope, diags);

    return end;
}
