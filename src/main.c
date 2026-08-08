// #include "cgllvm/codegen.h"
// #include "cgllvm/name_mangle.h"
#include "apfloat.h"
#include "apint.h"
#include "cmd.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/tokenize.h"
#include "literal.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/ast_log.h"
#include "parser/class.h"
#include "parser/type.h"
#include "position.h"
#include "sema/class_lit.h"
#include "sema/scope.h"
#include "symbol.h"
#include "types.h"
#include <assert.h>
#include <float.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");

    fseek(f, 0L, SEEK_END);
    mid_isize len = ftell(f);
    rewind(f);

    char *str = mid_malloc((len + 1) * sizeof(*str));
    str[len] = '\0';

    for (mid_isize i = 0; i < len; ++i)
        str[i] = fgetc(f);

    fclose(f);

    return str;
}

// removes duplicate diags
static void clean_diags(struct mid_DiagVec *diags)
{
    for (mid_isize i = diags->len - 1; i >= 1; --i) {
        auto cur = &diags->arr[i];
        auto prev = &diags->arr[i - 1];

        if (!mid_position_equal(&cur->pos, &prev->pos))
            continue;

        bool remove = false;

        if (cur->type == MIDDIAG_TYPE_ERROR) {
            if (prev->type == MIDDIAG_TYPE_NOTE && cur->err == prev->err)
                remove = true;
        } else if (cur->type == MIDDIAG_TYPE_WARNING) {
            if (prev->type == MIDDIAG_TYPE_WARNING && cur->warn == prev->warn)
                remove = true;
        }

        if (remove)
            midgen_dynremove(diags, i, middiag_deinit);
    }
}

// returns true if at least one of them was an error
static bool print_diags(struct mid_DiagVec *diags)
{
    clean_diags(diags);

    bool err = false;
    for (mid_isize i = 0; i < diags->len; ++i) {
        if (diags->arr[i].type == MIDDIAG_TYPE_ERROR)
            err = true;
        middiag_print(&diags->arr[i]);
    }

    return err;
}

static void log_tokens(const struct midlex_TokenVec *toks)
{
    for (mid_isize i = 0; i < toks->len; ++i) {
        printf("i = %" MID_PRIisz ", pos = (%d, %d), type = %d", i,
               toks->arr[i].pos.line, toks->arr[i].pos.column,
               toks->arr[i].type);
        if (toks->arr[i].type == MIDLEX_TOKENTYPE_INT_LIT) {
            printf(", value int = %" PRId64,
                   midint_to_sint(&toks->arr[i].val.i));
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_LONG_LIT) {
            printf(", value long = %" PRId64,
                   midint_to_sint(&toks->arr[i].val.i));
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_LONGLONG_LIT) {
            printf(", value long long = %" PRId64,
                   midint_to_sint(&toks->arr[i].val.i));
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_UINT_LIT) {
            printf(", value u int = %" PRId64,
                   midint_to_uint(&toks->arr[i].val.i));
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_ULONG_LIT) {
            printf(", value u long = %" PRId64,
                   midint_to_uint(&toks->arr[i].val.i));
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_ULONGLONG_LIT) {
            printf(", value u long long = %" PRId64,
                   midint_to_uint(&toks->arr[i].val.i));
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_FLOAT_LIT) {
            printf(", value f = ");
            midflt_print(&toks->arr[i].val.flt, stdout);
            printf("\n");
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_DOUBLE_LIT) {
            printf(", value d = ");
            midflt_print(&toks->arr[i].val.flt, stdout);
            printf("\n");
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_LONGDOUBLE_LIT) {
            printf(", value ld = ");
            midflt_print(&toks->arr[i].val.flt, stdout);
            printf("\n");
        } else if (toks->arr[i].type == MIDLEX_TOKENTYPE_STRING_LIT) {
            printf(", value str = '%s'", toks->arr[i].val.str.c);
        }
        printf("\n");
    }
}

static void log_symbols(const struct midsymb_Table *symtbl)
{
    for (mid_isize i = 0; i < symtbl->len; ++i)
        printf("symtbl[%" MID_PRIisz "] = '%s'\n", i, symtbl->arr[i]);
}

static void log_ast(const char *path, const struct midpar_ASTNode *root)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("can't open ast log file");
        return;
    }

    midpar_log_ast(root, f);

    fclose(f);
}

/*
static void test_mangling(const struct midsema_Scope *scope)
{
    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        struct midsema_Ident *ident = &scope->idents.arr[i];

        if (ident->type != MIDSEMA_IDENTTYPE_FUNC)
            continue;

        char *mangled = midllvm_mangle_func(&ident->decl->func_decl);
        printf("func at %d:%d becomes '%s'\n", ident->decl->start->pos.line,
               ident->decl->start->pos.column, mangled);
        free(mangled);
    }

    for (mid_isize i = 0; i < scope->childs.len; ++i)
        test_mangling(scope->childs.arr[i]);
}
*/

static void init_modules()
{
    midflt_init_module();
    midtype_init_module();
}

static void test_struct_lits(struct midpar_ASTNode *root)
{
    assert(root->root.arr[0]->type == MIDPAR_ASTNODETYPE_CLASS);
    struct midpar_Class *class = &root->root.arr[0]->class_;

    struct midsema_StructLit lit;
    assert(midsema_constexpr_default_init_struct(class, &lit));

    midsema_StructLit_deinit(&lit);
}

int main(int argc, char **argv)
{
    // enables unicode
    setlocale(LC_CTYPE, "en_US.UTF-8");

    init_modules();

    midcmd_init_args(argc, (const char *const *)argv);

    int ret = 0;

    struct midpar_Allocators allocs = {};

    assert(midcmd_get_args()->src);
    char *src = read_file(midcmd_get_args()->src);

    auto lex = midlex_tokenize(src, midcmd_get_args()->src);
    if (print_diags(&lex.diags)) {
        ret = 1;
        goto tokenize_failed;
    }

    if (midcmd_get_args()->log_tokens)
        log_tokens(&lex.toks);
    if (midcmd_get_args()->log_symbols)
        log_symbols(&lex.symtbl);

    struct mid_DiagVec parser_diags = midgen_dyninit();

    struct midpar_ASTNode root = {.type = MIDPAR_ASTNODETYPE_ROOT};
    struct midsema_Scope scope = {.type = MIDSEMA_SCOPETYPE_ROOT,
                                  .node = &root};

    for (mid_isize i = 0; lex.toks.arr[i].type != MIDLEX_TOKENTYPE_END;) {
        auto node =
            midpar_parse_node(lex.toks.arr, i, &i, &root, &scope,
                              (struct midpar_ParseNodeFlags){.skip_def = false},
                              &allocs, &parser_diags);
        midgen_dynpush(&root.root, node);
    }
    if (print_diags(&parser_diags)) {
        ret = 1;
        goto parser_failed;
    }

    if (midcmd_get_args()->ast_out)
        log_ast(midcmd_get_args()->ast_out, &root);

    test_struct_lits(&root);

    /*
    test_mangling(&scope);

    midllvm_init_codegen();
    midllvm_codegen(&root);
    */

parser_failed:
    // the root scope and root node need to be deallocated manually cuz they
    // weren't dynamically allocated
    midsema_Scope_deinit(&scope);
    midpar_ASTNode_deinit(&root);
    midgen_dyndeinit(&parser_diags, middiag_deinit);
tokenize_failed:
    midlex_Tokenize_deinit(&lex);
    free(src);

    midpar_Allocators_deinit(&allocs);
    return ret;
}
