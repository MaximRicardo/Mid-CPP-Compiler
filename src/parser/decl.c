#include "parser/decl.h"
#include "diag.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <assert.h>
#include <string.h>

// some parameters make telling the difference between a variable and a
// function impossible, like:
// A foo(B());
//       ^^^
static bool is_ambig_param(midlex_TokenIter start, midlex_TokenIter *out_end,
                           struct midsema_Scope *scope,
                           struct midpar_Allocators *allocs,
                           struct mid_DiagVec *diags)
{
    assert(midpar_valid_type_start(start, scope));

    struct midpar_ASTNode decl = {.type = MIDPAR_ASTNODETYPE_VAR_DECL,
                                  .start = start};
    // skip_init is true cuz the actual initializer isn't important, all that
    // really matters is whether or not there is one. it also avoids allocating
    // an expr tree to hold the initializer.
    midlex_TokenIter end = midpar_parse_var_decl(
        &decl.var_decl, start, MIDPAR_PARAM_ENDTYPES,
        (struct midpar_ParseVarDeclFlags){
            .add_to_scope = false, .single_inst = true, .skip_init = true},
        scope, allocs, diags);
    if (out_end)
        *out_end = end;

    auto inst = decl.var_decl.insts.arr[0];

    bool has_init = !inst->has_ctor && inst->init.start;

    bool has_dquals =
        memcmp(&inst->type.dquals.arr[0], &(struct midpar_TypeDataQual){},
               sizeof(inst->type.dquals.arr[0])) != 0;

    bool ret = !has_init && midsema_is_typespec_named(inst->type.spec) &&
               midsema_n_indir(&inst->type) == 0 && !inst->type.lv_ref &&
               !inst->type.rv_ref && !has_dquals;

    midpar_ASTNode_deinit(&decl);
    return ret;
}

static bool are_params_ambig(midlex_TokenIter lparen,
                             struct midsema_Scope *scope,
                             struct midpar_Allocators *allocs,
                             struct mid_DiagVec *diags)
{
    for (midlex_TokenIter i = lparen + 1; i->type != MIDLEX_TOKENTYPE_END;
         ++i) {
        if (i->type == MIDLEX_TOKENTYPE_ELLIPSIS)
            return false;
        else if (!is_ambig_param(i, &i, scope, allocs, diags))
            return false;

        if (i->type == MIDLEX_TOKENTYPE_R_PAREN ||
            i->type == MIDLEX_TOKENTYPE_END)
            break;
    }

    return true;
}

// does nothing if there isn't one
// Type operator+(const Type &a, const Type &b)
//              ^
//            start
static midlex_TokenIter skip_operator_overload(midlex_TokenIter start)
{
    if (start->type == MIDLEX_TOKENTYPE_L_SQBRACKET)
        return midpar_find_twin_sqbracket(start, nullptr) + 1;
    else if (start->type == MIDLEX_TOKENTYPE_L_PAREN) {
        return midpar_find_twin_paren(start, nullptr) + 1;
    } else if (midlex_is_op(start->type))
        return start + 1;
    else
        return start;
}

static bool valid_name_idx(midlex_TokenIter tok)
{
    return tok && tok->type == MIDLEX_TOKENTYPE_IDENTIFIER;
}

bool midpar_decl_is_func(midlex_TokenIter start, struct midsema_Scope *scope,
                         struct midpar_Allocators *allocs,
                         struct mid_DiagVec *diags, bool *out_mvp)
{
    assert(midpar_valid_type_start(start, scope));

    bool mvp = false;
    bool ret;

    midlex_TokenIter type_end;
    midlex_TokenIter name;
    auto type =
        midpar_parse_type(start, &type_end, scope, &name, false, allocs, diags);

    auto res =
        !name ? scope : midpar_parse_scope_res(name, &name, scope, diags);

    if (valid_name_idx(name) && !strcmp(name->ident, "operator"))
        type_end = skip_operator_overload(type_end);

    if (type_end->type != MIDLEX_TOKENTYPE_L_PAREN) {
        ret = false;
    } else {
        midlex_TokenIter lparen = type_end;

        if ((lparen + 1)->type == MIDLEX_TOKENTYPE_R_PAREN) {
            mvp = true;
            ret = true;
        } else if (midpar_valid_type_start(lparen + 1, res)) {
            mvp = are_params_ambig(lparen, res, allocs, diags);
            ret = true;
        } else {
            ret = false;
        }
    }

    if (out_mvp)
        *out_mvp = mvp;
    midpar_Type_deinit(&type);
    return ret;
}
