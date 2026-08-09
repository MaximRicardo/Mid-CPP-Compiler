#include "parser/scope.h"
#include "cmd.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "sema/scope.h"

static struct mid_Diag not_a_nmpace_err(const char *tok_name,
                                        const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("'%s' is not a namespace", tok_name),
        .err = MIDDIAG_ERR_MISSING_TOKEN,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static const struct midsema_Scope *
unary_scope_res(midlex_TokenIter start, midlex_TokenIter *out_end,
                const struct midsema_Scope *scope)
{
    if (out_end)
        *out_end = start + 1;

    auto ret = scope;
    while (ret->parent)
        ret = ret->parent;
    return ret;
}

static const struct midsema_Scope *
bin_scope_res(midlex_TokenIter start, midlex_TokenIter *out_end,
              const struct midsema_Scope *scope, struct mid_DiagVec *diags)
{
    const struct midsema_Scope *ret = midsema_closest_rnce_scope_const(scope);
    midlex_TokenIter i;
    bool name_err = false;
    for (i = start + 1; i->type == MIDLEX_TOKENTYPE_SCOPE_RES; i += 2) {
        auto ident = i - 1;

        if (ident->type != MIDLEX_TOKENTYPE_IDENTIFIER)
            midgen_dynpush(
                diags, middiag_expected_token_err("identifier", ident,
                                                  MIDDIAG_ERR_MISSING_TOKEN));
        const char *name = ident->ident;

        auto res = midsema_resolve_scope_const(name, ret);
        if (res) {
            ret = res;
        } else if (!name_err) {
            // don't print multiple errors cuz it could fill the console when
            // the only real problem is the first invalid scope
            midgen_dynpush(diags, not_a_nmpace_err(name, ident));
            name_err = true;
        }
    }

    if (out_end)
        *out_end = i - 1;
    return ret;
}

const struct midsema_Scope *
midpar_parse_scope_res_const(midlex_TokenIter start, midlex_TokenIter *out_end,
                             const struct midsema_Scope *scope,
                             struct mid_DiagVec *diags)
{
    if (start->type == MIDLEX_TOKENTYPE_SCOPE_RES) {
        return unary_scope_res(start, out_end, scope);
    } else if ((start + 1)->type == MIDLEX_TOKENTYPE_SCOPE_RES) {
        return bin_scope_res(start, out_end, scope, diags);
    } else {
        *out_end = start;
        return scope;
    }
}

struct midsema_Scope *midpar_parse_scope_res(midlex_TokenIter start,
                                             midlex_TokenIter *out_end,
                                             struct midsema_Scope *scope,
                                             struct mid_DiagVec *diags)
{
    return (struct midsema_Scope *)midpar_parse_scope_res_const(start, out_end,
                                                                scope, diags);
}

midlex_TokenIter midpar_skip_scope_res(midlex_TokenIter start)
{
    auto i = start->type == MIDLEX_TOKENTYPE_SCOPE_RES ? start : start + 1;

    for (; i->type == MIDLEX_TOKENTYPE_SCOPE_RES; i += 2)
        ;

    return i - 1;
}
