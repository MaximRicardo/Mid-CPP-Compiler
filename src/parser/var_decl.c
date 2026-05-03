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

isize_t Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                              const enum Lexer_TokenType *end_types,
                              isize_t n_end_types, struct Parser_VarDecl *decl,
                              const struct Parser_ASTNode *node, bool skip_init,
                              struct DiagVec *diags)
{
    *decl = (struct Parser_VarDecl){};

    isize_t type_end;
    decl->type =
        Parser_parse_type(toks, start, &type_end, node, &decl->name, diags);

    if (!decl->name) {
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
        decl->init_start = &toks[expr_start];
        if (skip_init) {
            return Parser_skip_expr(toks, expr_start, end_types, n_end_types,
                                    NULL);
        } else {
            decl->init = malloc(sizeof(*decl->init));
            isize_t end;
            *decl->init = Parser_parse_expr(toks, expr_start, end_types,
                                            n_end_types, &end, diags);
            return end;
        }
    }

    return type_end;
}
