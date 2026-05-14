#include "var_decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/type.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void Parser_VarDecl_deinit(struct Parser_VarDecl *self)
{
    Parser_Type_deinit(&self->type);
}

static struct Sema_Ident *add_ident(const struct Parser_VarDecl *decl,
                                    struct Parser_ASTNode *node,
                                    struct Sema_Scope *scope)
{

    return Sema_add_ident(
        scope, &(struct Sema_Ident){.name = decl->name,
                                    .decl = node,
                                    .def = NULL,
                                    .type = decl->type.squals.is_typedef
                                                ? SEMA_IDENTTYPE_TYPEDEF
                                                : SEMA_IDENTTYPE_VAR});
}

static void resolve_auto(struct Parser_VarDecl *decl)
{
    assert(decl->init);
    assert(decl->type.spec == PARSER_TYPESPEC_AUTO);
    assert(Parser_n_indir(&decl->type) == 0); // "auto *" not supported yet

    auto init_type = &decl->init->ret;

    decl->type.spec = init_type->spec;
    if (init_type->spec == PARSER_TYPESPEC_FPTR) {
        decl->type.fptr = mid_malloc(sizeof(*decl->type.fptr));
        *decl->type.fptr = Parser_copy_fptr_type(init_type->fptr);
    } else if (init_type->spec == PARSER_TYPESPEC_ARRAY) {
        decl->type.array = mid_malloc(sizeof(*decl->type.array));
        *decl->type.array = Parser_copy_array_type(init_type->array);
    } else if (Parser_is_typespec_named(init_type->spec)) {
        decl->type.named = init_type->named;
    }

    // the top most CV qualifier is discarded
    for (isize_t i = 1; i <= Parser_n_indir(init_type); ++i) {
        gen_dynpush(&decl->type.dquals, init_type->dquals.arr[i]);
    }
}

isize_t Parser_parse_var_def(const struct Lexer_Token *toks, isize_t start,
                             const enum Lexer_TokenType *end_types,
                             isize_t n_end_types, struct Parser_VarDecl *decl,
                             struct Sema_Scope *scope,
                             struct Parser_Allocators *allocs,
                             struct DiagVec *diags)
{
    gen_bumpmalloc(&allocs->expr, &decl->init);
    isize_t end;
    *decl->init = Parser_parse_expr(toks, start, end_types, n_end_types, &end,
                                    scope, diags);

    if (decl->type.spec == PARSER_TYPESPEC_AUTO)
        resolve_auto(decl);

    return end;
}

static struct Diag uninited_deduced_type_err(const char *name, const char *type,
                                             const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str(
            "declaration of '%s' as a deduced type '%s' needs an initializer",
            name, type),
        .err = ERRORTYPE_BAD_VAR_DECLARATION,
        .is_err = true,
    };
}

isize_t Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                              const enum Lexer_TokenType *end_types,
                              isize_t n_end_types, struct Parser_VarDecl *decl,
                              struct Parser_ASTNode *node,
                              struct Sema_Scope *scope, bool add_to_scope,
                              bool skip_init, struct Parser_Allocators *allocs,
                              struct DiagVec *diags)
{
    *decl = (struct Parser_VarDecl){};

    isize_t type_end;
    decl->type =
        Parser_parse_type(toks, start, &type_end, scope, &decl->name, diags);

    if (decl->name && add_to_scope && add_ident(decl, node, scope))
        gen_dynpush(diags, Diag_ident_redefined_err(decl->name, &toks[start],
                                                    ERRORTYPE_BAD_IDENTIFIER));

    isize_t assign_idx = type_end;
    bool has_init = toks[assign_idx].type == LEXER_TOKENTYPE_ASSIGN;

    if (has_init) {
        isize_t expr_start = assign_idx + 1;
        decl->init_start = &toks[expr_start];
        if (skip_init) {
            return Parser_skip_expr(toks, expr_start, end_types, n_end_types,
                                    NULL);
        }
        return Parser_parse_var_def(toks, expr_start, end_types, n_end_types,
                                    decl, scope, allocs, diags);
    } else if (decl->type.spec == PARSER_TYPESPEC_AUTO) {
        gen_dynpush(
            diags, uninited_deduced_type_err(decl->name, "auto", &toks[start]));
    }

    return type_end;
}
