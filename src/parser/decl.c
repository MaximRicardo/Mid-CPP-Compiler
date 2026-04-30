#include "decl.h"
#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/type.h"
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

    auto decl = Parser_parse_var_decl(toks, start, out_end, parent, diags);

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

bool Parser_decl_is_func(const struct Lexer_Token *toks, isize_t start,
                         const struct Parser_ASTNode *parent,
                         struct DiagVec *diags, bool *out_mvp)
{
    assert(Parser_valid_type_start(&toks[start], parent));

    bool mvp = false;
    bool ret;

    isize_t type_end;
    auto type = Parser_parse_type(toks, start, &type_end, parent, NULL, diags);

    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN) {
        printf("0\n");
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
