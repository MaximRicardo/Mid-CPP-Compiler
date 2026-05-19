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
static bool is_ambig_param(const struct Lexer_Token *toks, isize_t start,
                           isize_t *out_end, struct Sema_Scope *scope,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    assert(Parser_valid_type_start(toks, start, scope));

    struct Parser_ASTNode decl = {.type = PARSER_ASTNODETYPE_VAR_DECL,
                                  .start = &toks[start]};
    // skip_init is true cuz the actual initializer isn't important, all that
    // really matters is whether or not there is one. it also avoids allocating
    // an expr tree to hold the initializer.
    isize_t end = Parser_parse_var_decl(
        toks, start, PARSER_PARAM_ENDTYPES, &decl,
        (struct Parser_ParseVarDeclFlags){
            .add_to_scope = false, .single_inst = true, .skip_init = true},
        scope, allocs, diags);
    if (out_end)
        *out_end = end;

    auto inst = &decl.var_decl.insts.arr[0];

    bool has_init = !inst->has_ctor && inst->init.start;

    bool has_dquals =
        memcmp(&inst->type.dquals.arr[0], &(struct Parser_TypeDataQual){},
               sizeof(inst->type.dquals.arr[0])) != 0;

    bool ret = !has_init && Parser_is_typespec_named(inst->type.spec) &&
               Parser_n_indir(&inst->type) == 0 && !inst->type.lv_ref &&
               !inst->type.rv_ref && !has_dquals;

    Parser_ASTNode_deinit(&decl);
    return ret;
}

static bool are_params_ambig(const struct Lexer_Token *toks, isize_t lparen,
                             struct Sema_Scope *scope,
                             struct Parser_Allocators *allocs,
                             struct DiagVec *diags)
{
    for (isize_t i = lparen + 1; toks[i].type != LEXER_TOKENTYPE_END; ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_ELLIPSIS)
            return false;
        else if (!is_ambig_param(toks, i, &i, scope, allocs, diags))
            return false;

        if (toks[i].type == LEXER_TOKENTYPE_R_PAREN ||
            toks[i].type == LEXER_TOKENTYPE_END)
            break;
    }

    return true;
}

// does nothing if there isn't one
// Type operator+(const Type &a, const Type &b)
//              ^
//            start
static isize_t skip_operator_overload(const struct Lexer_Token *toks,
                                      isize_t start)
{
    if (toks[start].type == LEXER_TOKENTYPE_L_SQBRACKET)
        return Parser_find_twin_sqbracket(toks, start, ISIZE_MAX) + 1;
    else if (toks[start].type == LEXER_TOKENTYPE_L_PAREN) {
        return Parser_find_twin_paren(toks, start, ISIZE_MAX) + 1;
    } else if (Lexer_is_op(toks[start].type))
        return start + 1;
    else
        return start;
}

static bool valid_name_idx(isize_t idx, const struct Lexer_Token *toks)
{
    return idx != -1 && toks[idx].type == LEXER_TOKENTYPE_IDENTIFIER;
}

bool Parser_decl_is_func(const struct Lexer_Token *toks, isize_t start,
                         struct Sema_Scope *scope,
                         struct Parser_Allocators *allocs,
                         struct DiagVec *diags, bool *out_mvp)
{
    assert(Parser_valid_type_start(toks, start, scope));

    bool mvp = false;
    bool ret;

    isize_t type_end;
    isize_t name;
    auto type = Parser_parse_type(toks, start, &type_end, scope, &name, diags);

    auto res = name == -1
                   ? scope
                   : Parser_parse_scope_res(toks, name, &name, scope, diags);

    if (valid_name_idx(name, toks) && !strcmp(toks[name].ident, "operator"))
        type_end = skip_operator_overload(toks, type_end);

    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN) {
        ret = false;
    } else {
        isize_t lparen = type_end;

        if (toks[lparen + 1].type == LEXER_TOKENTYPE_R_PAREN) {
            mvp = true;
            ret = true;
        } else if (Parser_valid_type_start(toks, lparen + 1, res)) {
            mvp = are_params_ambig(toks, lparen, res, allocs, diags);
            ret = true;
        } else {
            ret = false;
        }
    }

    if (out_mvp)
        *out_mvp = mvp;
    Parser_Type_deinit(&type);
    return ret;
}
