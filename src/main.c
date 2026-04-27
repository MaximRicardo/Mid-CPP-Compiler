#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/tokenize.h"
#include "parser/type.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");

    fseek(f, 0L, SEEK_END);
    isize_t len = ftell(f);
    rewind(f);

    char *str = malloc((len + 1) * sizeof(*str));
    str[len] = '\0';

    for (isize_t i = 0; i < len; ++i)
        str[i] = fgetc(f);

    fclose(f);

    return str;
}

// returns true if at least one of them was an error
bool print_diags(const struct DiagVec *diags)
{
    bool err = false;
    for (isize_t i = 0; i < diags->len; ++i) {
        if (diags->arr[i].is_err)
            err = true;
        Diag_print(&diags->arr[i]);
    }

    return err;
}

int main(int argc, char **argv)
{
    int ret = 0;

    assert(argc >= 2);
    char *src = read_file(argv[1]);

    auto lex = Lexer_tokenize(src, argv[1]);
    if (print_diags(&lex.diags))
        goto tokenize_failed;

    /*
    for (isize_t i = 0; i < toks.len; ++i) {
        printf("i = %" PRIisz ", pos = (%d, %d), type = %d", i,
               toks.arr[i].pos.line, toks.arr[i].pos.column, toks.arr[i].type);
        if (toks.arr[i].type == LEXER_TOKENTYPE_INT_LIT)
            printf(", value int = %" PRId64, toks.arr[i].val.s_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_LONG_LIT)
            printf(", value long = %" PRId64, toks.arr[i].val.s_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_LONGLONG_LIT)
            printf(", value long long = %" PRId64, toks.arr[i].val.s_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_UINT_LIT)
            printf(", value u int = %" PRId64, toks.arr[i].val.u_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_ULONG_LIT)
            printf(", value u long = %" PRId64, toks.arr[i].val.u_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_ULONGLONG_LIT)
            printf(", value u long long = %" PRId64, toks.arr[i].val.u_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_FLOAT_LIT)
            printf(", value f = %f", toks.arr[i].val.f_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_DOUBLE_LIT)
            printf(", value d = %lf", toks.arr[i].val.d_lit);
        else if (toks.arr[i].type == LEXER_TOKENTYPE_LONGDOUBLE_LIT)
            printf(", value ld = %Lf", toks.arr[i].val.ld_lit);
        printf("\n");
    }
    */

    struct DiagVec parser_diags = gen_dyninit();

    /*
    auto expr = Parser_parse_expr(lex.toks.arr, 0, lex.toks.len, &parser_diags);
    if (print_diags(&parser_diags))
        goto parser_failed;

    printf("expr = %" PRId64 "\n", Parser_evaluate(&expr).sint);
    */

    auto type = Parser_parse_type(lex.toks.arr, 0, NULL, &parser_diags);
    if (print_diags(&parser_diags))
        goto parser_failed;

    printf("type spec = %d\n", type.spec);
    for (isize_t i = 0; i < type.mods.len; ++i)
        printf("mod[%" PRIisz "] = %d\n", i, type.mods.arr[i]);

parser_failed:
    Parser_Type_deinit(&type);
    gen_dyndeinit(&parser_diags, Diag_deinit);
tokenize_failed:
    Lexer_Tokenize_deinit(&lex);
    free(src);
    return ret;
}
