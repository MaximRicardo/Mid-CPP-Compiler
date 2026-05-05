#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/tokenize.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/type.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path)
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
static bool print_diags(const struct DiagVec *diags)
{
    bool err = false;
    for (isize_t i = 0; i < diags->len; ++i) {
        if (diags->arr[i].is_err)
            err = true;
        Diag_print(&diags->arr[i]);
    }

    return err;
}

/*
static void free_str(char **str)
{
    free(*str);
}

static void test()
{
    gen_bumpalloc_struct_named(BumpAlloc, char *);

    struct BumpAlloc handle = gen_bumpinit();

    char **str[1024];
    for (size_t i = 0; i < ARRLEN(str); ++i) {
        gen_bumpcalloc(&handle, &str[i]);
        *str[i] = Print_fmt_to_str("string nr %zu", i);
    }

    for (size_t i = 0; i < ARRLEN(str); ++i)
        printf("*str[%zu] = %s\n", i, *str[i]);

    gen_bumpdeinit(&handle, free_str);
}
*/

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

    struct Parser_ASTNode root = {.type = PARSER_ASTNODETYPE_ROOT};
    struct Sema_Scope scope = {.type = SEMA_SCOPETYPE_ROOT, .node = &root};

    for (isize_t i = 0; lex.toks.arr[i].type != LEXER_TOKENTYPE_END;) {
        printf("looping at %d:%d, %" PRIisz "/%" PRIisz "\n",
               lex.toks.arr[i].pos.line, lex.toks.arr[i].pos.column, i,
               lex.toks.len - 1);
        auto node = Parser_parse_node(lex.toks.arr, i, &i, &root, &scope, false,
                                      &parser_diags);
        gen_dynpush(&root.root, node);
    }
    if (print_diags(&parser_diags))
        goto parser_failed;

    struct DiagVec sema_diags = gen_dyninit();
    Sema_typecheck_root(&root, &scope, &sema_diags);
    if (print_diags(&sema_diags))
        goto sema_failed;

sema_failed:
    gen_dyndeinit(&sema_diags, Diag_deinit);
parser_failed:
    Sema_Scope_deinit(&scope);
    Parser_ASTNode_deinit(&root);
    gen_dyndeinit(&parser_diags, Diag_deinit);
tokenize_failed:
    Lexer_Tokenize_deinit(&lex);
    free(src);

    return ret;
}
