#include "cgllvm/codegen.h"
#include "cgllvm/name_mangle.h"
#include "cmd.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/tokenize.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/ast_log.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "symbol.h"
#include <assert.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");

    fseek(f, 0L, SEEK_END);
    isize_t len = ftell(f);
    rewind(f);

    char *str = mid_malloc((len + 1) * sizeof(*str));
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

static void log_tokens(const struct Lexer_TokenVec *toks)
{
    for (isize_t i = 0; i < toks->len; ++i) {
        printf("i = %" PRIisz ", pos = (%d, %d), type = %d", i,
               toks->arr[i].pos.line, toks->arr[i].pos.column,
               toks->arr[i].type);
        if (toks->arr[i].type == LEXER_TOKENTYPE_INT_LIT)
            printf(", value int = %" PRId64, toks->arr[i].val.sint);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_LONG_LIT)
            printf(", value long = %" PRId64, toks->arr[i].val.sint);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_LONGLONG_LIT)
            printf(", value long long = %" PRId64, toks->arr[i].val.sint);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_UINT_LIT)
            printf(", value u int = %" PRId64, toks->arr[i].val.uint);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_ULONG_LIT)
            printf(", value u long = %" PRId64, toks->arr[i].val.uint);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_ULONGLONG_LIT)
            printf(", value u long long = %" PRId64, toks->arr[i].val.uint);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_FLOAT_LIT)
            printf(", value f = %Lf", toks->arr[i].val.flt);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_DOUBLE_LIT)
            printf(", value d = %Lf", toks->arr[i].val.flt);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_LONGDOUBLE_LIT)
            printf(", value ld = %Lf", toks->arr[i].val.flt);
        else if (toks->arr[i].type == LEXER_TOKENTYPE_STRING_LIT)
            printf(", value str = '%s'", toks->arr[i].val.str.c);
        printf("\n");
    }
}

static void log_symbols(const struct SymbolTable *symtbl)
{
    for (isize_t i = 0; i < symtbl->len; ++i)
        printf("symtbl[%" PRIisz "] = '%s'\n", i, symtbl->arr[i]);
}

static void log_ast(const char *path, const struct Parser_ASTNode *root)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("can't open ast log file");
        return;
    }

    Parser_log_ast(root, f);

    fclose(f);
}

static void test_mangling(const struct Sema_Scope *scope)
{
    for (isize_t i = 0; i < scope->idents.len; ++i) {
        struct Sema_Ident *ident = &scope->idents.arr[i];

        if (ident->type != SEMA_IDENTTYPE_FUNC)
            continue;

        char *mangled = CGLLVM_mangle_func(&ident->decl->func_decl);
        printf("func at %d:%d becomes '%s'\n", ident->decl->start->pos.line,
               ident->decl->start->pos.column, mangled);
        free(mangled);
    }

    for (isize_t i = 0; i < scope->childs.len; ++i)
        test_mangling(scope->childs.arr[i]);
}

int main(int argc, char **argv)
{
    // enables unicode
    setlocale(LC_CTYPE, "en_US.UTF-8");

    CMD_init_args(argc, argv);

    int ret = 0;

    struct Parser_Allocators allocs = {};

    assert(CMD_get_args()->src);
    char *src = read_file(CMD_get_args()->src);

    auto lex = Lexer_tokenize(src, CMD_get_args()->src);
    if (print_diags(&lex.diags)) {
        ret = 1;
        goto tokenize_failed;
    }

    if (CMD_get_args()->log_tokens)
        log_tokens(&lex.toks);
    if (CMD_get_args()->log_symbols)
        log_symbols(&lex.symtbl);

    struct DiagVec parser_diags = gen_dyninit();

    struct Parser_ASTNode root = {.type = PARSER_ASTNODETYPE_ROOT};
    struct Sema_Scope scope = {.type = SEMA_SCOPETYPE_ROOT, .node = &root};

    for (isize_t i = 0; lex.toks.arr[i].type != LEXER_TOKENTYPE_END;) {
        auto node =
            Parser_parse_node(lex.toks.arr, i, &i, &root, &scope,
                              (struct Parser_ParseNodeFlags){.skip_def = false},
                              &allocs, &parser_diags);
        gen_dynpush(&root.root, node);
    }
    if (print_diags(&parser_diags)) {
        ret = 1;
        goto parser_failed;
    }

    if (CMD_get_args()->ast_out)
        log_ast(CMD_get_args()->ast_out, &root);

    test_mangling(&scope);

    CGLLVM_init_codegen();
    CGLLVM_codegen(&root);

parser_failed:
    // the root scope and root node need to be deallocated manually cuz they
    // weren't dynamically allocated
    Sema_Scope_deinit(&scope);
    Parser_ASTNode_deinit(&root);
    gen_dyndeinit(&parser_diags, Diag_deinit);
tokenize_failed:
    Lexer_Tokenize_deinit(&lex);
    free(src);

    Parser_Allocators_deinit(&allocs);
    return ret;
}
