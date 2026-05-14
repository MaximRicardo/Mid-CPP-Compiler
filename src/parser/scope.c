#include "scope.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "print.h"
#include "sema/scope.h"

static struct Diag not_a_nmpace_err(const char *tok_name,
                                    const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("'%s' is not a namespace", tok_name),
        .err = ERRORTYPE_MISSING_TOKEN,
        .is_err = true,
    };
}

static struct Sema_Scope *unary_scope_res(isize_t start, isize_t *out_end,
                                          struct Sema_Scope *scope)
{
    if (out_end)
        *out_end = start + 1;

    struct Sema_Scope *ret = scope;
    while (ret->parent)
        ret = ret->parent;
    return ret;
}

static struct Sema_Scope *bin_scope_res(const struct Lexer_Token *toks,
                                        isize_t start, isize_t *out_end,
                                        struct Sema_Scope *scope,
                                        struct DiagVec *diags)
{
    struct Sema_Scope *ret = Sema_closest_rnce_scope(scope);
    isize_t i;
    bool name_err = false;
    for (i = start + 1; toks[i].type == LEXER_TOKENTYPE_SCOPE_RES; i += 2) {
        isize_t ident = i - 1;

        if (toks[ident].type != LEXER_TOKENTYPE_IDENTIFIER)
            gen_dynpush(diags,
                        Diag_expected_token_err("identifier", &toks[ident],
                                                ERRORTYPE_MISSING_TOKEN));
        const char *name = toks[ident].ident;

        auto res = Sema_resolve_scope(name, ret);
        if (res) {
            ret = res;
        } else if (!name_err) {
            // don't print multiple errors cuz it could fill the console when
            // the only real problem is the first invalid scope
            gen_dynpush(diags, not_a_nmpace_err(name, &toks[ident]));
            name_err = true;
        }
    }

    if (out_end)
        *out_end = i - 1;
    return ret;
}

struct Sema_Scope *Parser_parse_scope_res(const struct Lexer_Token *toks,
                                          isize_t start, isize_t *out_end,
                                          struct Sema_Scope *scope,
                                          struct DiagVec *diags)
{
    if (toks[start].type == LEXER_TOKENTYPE_SCOPE_RES)
        return unary_scope_res(start, out_end, scope);
    else
        return bin_scope_res(toks, start, out_end, scope, diags);
}
