#include "var_decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/bump.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
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

static struct Diag expected_ident_err(const struct Lexer_Token *tok)
{
    return (struct Diag){.pos = tok->pos,
                         .line = tok->line,
                         .msg = Print_fmt_to_str("expected identifier"),
                         .err = ERRORTYPE_MISSING_IDENTIFIER,
                         .is_err = true};
}

static struct Diag redefined_ident_err(const struct Lexer_Token *tok,
                                       const char *name)
{
    return (struct Diag){.pos = tok->pos,
                         .line = tok->line,
                         .msg = Print_fmt_to_str("'%s' redefined", name),
                         .err = ERRORTYPE_BAD_IDENTIFIER,
                         .is_err = true};
}

isize_t Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                              const enum Lexer_TokenType *end_types,
                              isize_t n_end_types, struct Parser_VarDecl *decl,
                              struct Parser_ASTNode *node,
                              struct Sema_Scope *scope, bool add_to_scope,
                              bool skip_init, struct DiagVec *diags)
{
    *decl = (struct Parser_VarDecl){};

    isize_t type_end;
    decl->type =
        Parser_parse_type(toks, start, &type_end, scope, &decl->name, diags);

    if (!decl->name) {
        gen_dynpush(diags, expected_ident_err(&toks[start]));
    } else if (add_to_scope &&
               Sema_add_ident(scope, &(struct Sema_Ident){
                                         .name = decl->name,
                                         .decl = node,
                                         .def = NULL,
                                         .type = decl->type.squals.is_typedef
                                                     ? SEMA_IDENTTYPE_TYPEDEF
                                                     : SEMA_IDENTTYPE_VAR})) {
        gen_dynpush(diags, redefined_ident_err(&toks[start], decl->name));
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
            gen_bumpmalloc(&Parser_bumps.expr, &decl->init);
            isize_t end;
            *decl->init = Parser_parse_expr(toks, expr_start, end_types,
                                            n_end_types, &end, diags);
            return end;
        }
    }

    return type_end;
}
