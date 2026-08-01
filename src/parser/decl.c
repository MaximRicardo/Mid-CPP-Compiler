#include "decl.h"
#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"
#include <assert.h>
#include <string.h>

// some parameters make telling the difference between a variable and a
// function impossible, like:
// A foo(B());
//       ^^^
static bool is_ambig_param(const struct MidLexer_Token *toks, mid_isize start,
                           mid_isize *out_end, struct MidSema_Scope *scope,
                           struct MidParser_Allocators *allocs,
                           struct MidDiag_DiagVec *diags)
{
    assert(MidParser_valid_type_start(toks, start, scope));

    struct MidParser_ASTNode decl = {.type = MIDPARSER_ASTNODETYPE_VAR_DECL,
                                     .start = &toks[start]};
    // skip_init is true cuz the actual initializer isn't important, all that
    // really matters is whether or not there is one. it also avoids allocating
    // an expr tree to hold the initializer.
    mid_isize end = MidParser_parse_var_decl(
        &decl.var_decl, toks, start, MIDPARSER_PARAM_ENDTYPES,
        (struct MidParser_ParseVarDeclFlags){
            .add_to_scope = false, .single_inst = true, .skip_init = true},
        scope, allocs, diags);
    if (out_end)
        *out_end = end;

    auto inst = decl.var_decl.insts.arr[0];

    bool has_init = !inst->has_ctor && inst->init.start;

    bool has_dquals =
        memcmp(&inst->type.dquals.arr[0], &(struct MidParser_TypeDataQual){},
               sizeof(inst->type.dquals.arr[0])) != 0;

    bool ret = !has_init && MidParser_is_typespec_named(inst->type.spec) &&
               MidParser_n_indir(&inst->type) == 0 && !inst->type.lv_ref &&
               !inst->type.rv_ref && !has_dquals;

    MidParser_ASTNode_deinit(&decl);
    return ret;
}

static bool are_params_ambig(const struct MidLexer_Token *toks,
                             mid_isize lparen, struct MidSema_Scope *scope,
                             struct MidParser_Allocators *allocs,
                             struct MidDiag_DiagVec *diags)
{
    for (mid_isize i = lparen + 1; toks[i].type != MIDLEXER_TOKENTYPE_END;
         ++i) {
        if (toks[i].type == MIDLEXER_TOKENTYPE_ELLIPSIS)
            return false;
        else if (!is_ambig_param(toks, i, &i, scope, allocs, diags))
            return false;

        if (toks[i].type == MIDLEXER_TOKENTYPE_R_PAREN ||
            toks[i].type == MIDLEXER_TOKENTYPE_END)
            break;
    }

    return true;
}

// does nothing if there isn't one
// Type operator+(const Type &a, const Type &b)
//              ^
//            start
static mid_isize skip_operator_overload(const struct MidLexer_Token *toks,
                                        mid_isize start)
{
    if (toks[start].type == MIDLEXER_TOKENTYPE_L_SQBRACKET)
        return MidParser_find_twin_sqbracket(toks, start, MID_ISIZE_MAX) + 1;
    else if (toks[start].type == MIDLEXER_TOKENTYPE_L_PAREN) {
        return MidParser_find_twin_paren(toks, start, MID_ISIZE_MAX) + 1;
    } else if (MidLexer_is_op(toks[start].type))
        return start + 1;
    else
        return start;
}

static bool valid_name_idx(mid_isize idx, const struct MidLexer_Token *toks)
{
    return idx != -1 && toks[idx].type == MIDLEXER_TOKENTYPE_IDENTIFIER;
}

bool MidParser_decl_is_func(const struct MidLexer_Token *toks, mid_isize start,
                            struct MidSema_Scope *scope,
                            struct MidParser_Allocators *allocs,
                            struct MidDiag_DiagVec *diags, bool *out_mvp)
{
    assert(MidParser_valid_type_start(toks, start, scope));

    bool mvp = false;
    bool ret;

    mid_isize type_end;
    mid_isize name;
    auto type = MidParser_parse_type(toks, start, &type_end, scope, &name,
                                     false, allocs, diags);

    auto res = name == -1
                   ? scope
                   : MidParser_parse_scope_res(toks, name, &name, scope, diags);

    if (valid_name_idx(name, toks) && !strcmp(toks[name].ident, "operator"))
        type_end = skip_operator_overload(toks, type_end);

    if (toks[type_end].type != MIDLEXER_TOKENTYPE_L_PAREN) {
        ret = false;
    } else {
        mid_isize lparen = type_end;

        if (toks[lparen + 1].type == MIDLEXER_TOKENTYPE_R_PAREN) {
            mvp = true;
            ret = true;
        } else if (MidParser_valid_type_start(toks, lparen + 1, res)) {
            mvp = are_params_ambig(toks, lparen, res, allocs, diags);
            ret = true;
        } else {
            ret = false;
        }
    }

    if (out_mvp)
        *out_mvp = mvp;
    MidParser_Type_deinit(&type);
    return ret;
}
