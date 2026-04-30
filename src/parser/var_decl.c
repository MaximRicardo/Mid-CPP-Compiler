#include "var_decl.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "print.h"
#include <stddef.h>
#include <stdlib.h>

void Parser_VarDecl_deinit(struct Parser_VarDecl *self)
{
    Parser_Type_deinit(&self->type);
    if (self->init) {
        Parser_Expr_deinit(self->init);
        free(self->init);
    }
}

struct Parser_VarDecl
Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                      isize_t *out_end, const enum Lexer_TokenType *end_types,
                      isize_t n_end_types, const struct Parser_ASTNode *parent,
                      struct DiagVec *diags)
{
    struct Parser_VarDecl ret = {};
    isize_t type_end;
    ret.type =
        Parser_parse_type(toks, start, &type_end, parent, &ret.name, diags);

    if (!ret.name) {
        struct Diag err = {.pos = toks[start].pos,
                           .line = toks[start].line,
                           .msg = Print_fmt_to_str("expected identifier"),
                           .err = ERRORTYPE_MISSING_IDENTIFIER,
                           .is_err = true};
        gen_dynpush(diags, err);
    }

    isize_t assign_idx = type_end;
    bool has_init = toks[assign_idx].type == LEXER_TOKENTYPE_ASSIGN;

    if (has_init) {
        isize_t expr_start = assign_idx + 1;
        ret.init = malloc(sizeof(*ret.init));
        *ret.init = Parser_parse_expr(toks, expr_start, end_types, n_end_types,
                                      out_end, diags);
    } else if (out_end) {
        *out_end = type_end;
    }

    return ret;
}
