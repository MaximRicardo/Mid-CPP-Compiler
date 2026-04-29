#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/tokenize.h"
#include "parser/ast.h"
#include "parser/astvec.h"
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

isize_t find_semicolon(const struct Lexer_Token *toks, isize_t start,
                       isize_t end)
{
    for (isize_t i = start; i < end; ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_SEMICOLON)
            return i;
    }

    return -1;
}

int main(int argc, char **argv)
{
    int ret = 0;

    assert(argc >= 2);
    char *src = read_file(argv[1]);

    auto lex = Lexer_tokenize(src, argv[1]);
    if (print_diags(&lex.diags))
        goto tokenize_failed;

    for (isize_t i = 0; i < lex.toks.len; ++i) {
        printf("i = %" PRIisz ", pos = (%d, %d), type = %d", i,
               lex.toks.arr[i].pos.line, lex.toks.arr[i].pos.column,
               lex.toks.arr[i].type);
        if (lex.toks.arr[i].type == LEXER_TOKENTYPE_INT_LIT)
            printf(", value int = %" PRId64, lex.toks.arr[i].val.sint);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_LONG_LIT)
            printf(", value long = %" PRId64, lex.toks.arr[i].val.sint);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_LONGLONG_LIT)
            printf(", value long long = %" PRId64, lex.toks.arr[i].val.sint);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_UINT_LIT)
            printf(", value u int = %" PRId64, lex.toks.arr[i].val.uint);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_ULONG_LIT)
            printf(", value u long = %" PRId64, lex.toks.arr[i].val.uint);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_ULONGLONG_LIT)
            printf(", value u long long = %" PRId64, lex.toks.arr[i].val.uint);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_FLOAT_LIT)
            printf(", value f = %f", lex.toks.arr[i].val.flt);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_DOUBLE_LIT)
            printf(", value d = %lf", lex.toks.arr[i].val.dbl);
        else if (lex.toks.arr[i].type == LEXER_TOKENTYPE_LONGDOUBLE_LIT)
            printf(", value ld = %Lf", lex.toks.arr[i].val.l_dbl);
        printf("\n");
    }

    for (isize_t i = 0; i < lex.symtbl.len; ++i) {
        printf("symtbl[%" PRIisz "] = '%s'\n", i, lex.symtbl.arr[i]);
    }

    struct DiagVec parser_diags = gen_dyninit();

    /*
    auto decl = Parser_parse_vardecl(
        lex.toks.arr, 0, find_semicolon(lex.toks.arr, 0, lex.toks.len),
        &parser_diags);
    if (print_diags(&parser_diags))
        goto parser_failed;

    printf("decl name = %s\n", decl.name);
    printf("type spec = %d\n", decl.type.spec);
    printf("is_static = %d\n", decl.type.squals.is_static);
    printf("is_constexpr = %d\n", decl.type.squals.is_constexpr);
    printf("is_lv_ref = %d\n", decl.type.lv_ref);
    printf("is_rv_ref = %d\n", decl.type.rv_ref);
    for (isize_t i = 0; i < decl.type.dquals.len; ++i)
        printf("type is_const[%" PRIisz "] = %d\n", i,
               decl.type.dquals.arr[i].is_const);
    */

    struct Parser_ASTNode root = {};
    root.type = PARSER_ASTNODETYPE_ROOT;

    for (isize_t i = 0; lex.toks.arr[i].type != LEXER_TOKENTYPE_END;) {
        printf("looping at %d:%d\n", lex.toks.arr[i].pos.line,
               lex.toks.arr[i].pos.column);
        auto node =
            Parser_parse_node(lex.toks.arr, i, &i, &root, &parser_diags);
        gen_dynpush(&root.root, node);
    }
    if (print_diags(&parser_diags))
        goto parser_failed;

    /*
    printf("n indirs = %" PRIisz
           ", is const = %d, is typedef = %d, lv_ref = %d\n",
           root.root.arr[1].var_decl.type.dquals.len - 1,
           root.root.arr[1].var_decl.type.dquals.arr[0].is_const,
           root.root.arr[1].var_decl.type.squals.is_typedef,
           root.root.arr[1].var_decl.type.lv_ref);
    */
    {
        char *type_str = Parser_type_to_str(&root.root.arr[1].var_decl.type);
        printf("type = '%s'\n", type_str);
        free(type_str);
    }

parser_failed:
    Parser_ASTNode_deinit(&root);
    gen_dyndeinit(&parser_diags, Diag_deinit);
tokenize_failed:
    Lexer_Tokenize_deinit(&lex);
    free(src);
    return ret;
}
