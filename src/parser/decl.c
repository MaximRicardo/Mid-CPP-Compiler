#include "decl.h"
#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// some parameters make telling the difference between a variable and a
// function impossible, like:
// A foo(B());
//       ^^^
static bool is_ambig_param(const struct Lexer_Token *toks, isize_t start,
                           isize_t *out_end,
                           const struct Parser_ASTNode *parent,
                           struct DiagVec *diags)
{
    assert(Parser_valid_type_start(&toks[start], parent));

    struct Parser_VarDecl decl;
    isize_t end = Parser_parse_var_decl(toks, start, PARSER_PARAM_ENDTYPES,
                                        &decl, parent, false, diags);
    if (out_end)
        *out_end = end;

    bool has_dquals =
        memcmp(&decl.type.dquals.arr[0], &(struct Parser_TypeDataQual){},
               sizeof(decl.type.dquals.arr[0])) != 0;

    bool ret = decl.init == NULL && Parser_is_typespec_named(decl.type.spec) &&
               Parser_n_indir(&decl.type) == 0 && !decl.type.lv_ref &&
               !decl.type.rv_ref && !has_dquals;

    Parser_VarDecl_deinit(&decl);
    return ret;
}

static bool are_params_ambig(const struct Lexer_Token *toks, isize_t lparen,
                             const struct Parser_ASTNode *parent,
                             struct DiagVec *diags)
{
    for (isize_t i = lparen + 1; toks[i].type != LEXER_TOKENTYPE_END; ++i) {
        if (!is_ambig_param(toks, i, &i, parent, diags))
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

bool Parser_decl_is_func(const struct Lexer_Token *toks, isize_t start,
                         const struct Parser_ASTNode *parent,
                         struct DiagVec *diags, bool *out_mvp)
{
    assert(Parser_valid_type_start(&toks[start], parent));

    bool mvp = false;
    bool ret;

    isize_t type_end;
    const char *name;
    auto type = Parser_parse_type(toks, start, &type_end, parent, &name, diags);
    if (name && !strcmp(name, "operator"))
        type_end = skip_operator_overload(toks, type_end);

    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN) {
        ret = false;
        goto finish;
    }

    isize_t lparen = type_end;

    if (toks[lparen + 1].type == LEXER_TOKENTYPE_R_PAREN) {
        mvp = true;
        ret = true;
    } else if (Parser_valid_type_start(&toks[lparen + 1], parent)) {
        mvp = are_params_ambig(toks, lparen, parent, diags);
        ret = true;
    } else {
        ret = false;
    }

finish:
    if (out_mvp)
        *out_mvp = mvp;
    Parser_Type_deinit(&type);
    return ret;
}
