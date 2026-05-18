#include "ast.h"
#include "allocator.h"
#include "decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/class.h"
#include "parser/end_types.h"
#include "parser/enum.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/namespace.h"
#include "parser/return.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"
#include <stdio.h>
#include <string.h>

void Parser_ASTNode_deinit(struct Parser_ASTNode *self)
{
    switch (self->type) {
    case PARSER_ASTNODETYPE_ROOT:
        gen_dyndeinit(&self->root);
        break;

    case PARSER_ASTNODETYPE_EXPR:
        Parser_Expr_deinit(&self->expr);
        break;

    case PARSER_ASTNODETYPE_VAR_DECL:
        Parser_VarDecl_deinit(&self->var_decl);
        break;

    case PARSER_ASTNODETYPE_FUNC_DECL:
        Parser_FuncDecl_deinit(&self->func_decl);
        break;

    case PARSER_ASTNODETYPE_ENUM:
        Parser_Enum_deinit(&self->enum_);
        break;

    case PARSER_ASTNODETYPE_CLASS:
        Parser_Class_deinit(&self->class_);
        break;

    case PARSER_ASTNODETYPE_NAMESPACE:
        Parser_Namespace_deinit(&self->nmspace);
        break;

    case PARSER_ASTNODETYPE_RETURN:
        break;
    }
}

static bool is_class_start(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_CLASS || type == LEXER_TOKENTYPE_STRUCT ||
           type == LEXER_TOKENTYPE_UNION;
}

static isize_t skip_typequals(const struct Lexer_Token *toks, isize_t start)
{
    isize_t i = start;
    while (Lexer_is_typequal(toks[i++].type))
        ;

    return i - 1;
}

static bool is_ctor_start(const struct Lexer_Token *toks, isize_t start,
                          const struct Parser_ASTNode *parent)
{
    if (toks[start].type != LEXER_TOKENTYPE_IDENTIFIER)
        return false;
    if (toks[start + 1].type != LEXER_TOKENTYPE_L_PAREN)
        return false;

    assert(parent->type == PARSER_ASTNODETYPE_CLASS);
    return !strcmp(toks[start].ident, parent->class_.name);
}

static bool is_dtor_start(const struct Lexer_Token *tok)
{
    return tok->type == LEXER_TOKENTYPE_BITWISE_NOT; // '~'
}

struct Parser_ASTNode *
Parser_parse_node(const struct Lexer_Token *toks, isize_t start,
                  isize_t *out_end, struct Parser_ASTNode *parent,
                  struct Sema_Scope *scope, struct Parser_ParseNodeFlags flags,
                  struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    struct Parser_ASTNode *ret;
    gen_bumpmalloc(&allocs->ast, &ret);
    *ret = (struct Parser_ASTNode){.start = &toks[start], .parent = parent};

    printf("AST START AT %d:%d\n", ret->start->pos.line,
           ret->start->pos.column);

    isize_t check_type = skip_typequals(toks, start);
    isize_t end;
    bool check_semi = true;
    if (is_class_start(toks[check_type].type)) {
        printf("CLASS NODE\n");
        ret->type = PARSER_ASTNODETYPE_CLASS;
        end = Parser_parse_class(ret, scope, toks, start, flags.skip_def,
                                 allocs, diags);
    } else if (flags.is_field && is_ctor_start(toks, check_type, parent)) {
        printf("CTOR NODE\n");
        ret->type = PARSER_ASTNODETYPE_FUNC_DECL;
        end = Parser_parse_tor(toks, start, ret, scope, flags.skip_def, allocs,
                               diags);
        check_semi = !ret->func_decl.has_def;
    } else if (flags.is_field && is_dtor_start(&toks[check_type])) {
        printf("DTOR NODE\n");
        ret->type = PARSER_ASTNODETYPE_FUNC_DECL;
        end = Parser_parse_tor(toks, start, ret, scope, flags.skip_def, allocs,
                               diags);
        check_semi = !ret->func_decl.has_def;
    } else if (toks[check_type].type == LEXER_TOKENTYPE_NAMESPACE) {
        printf("NAMESPACE NODE\n");
        check_semi = false;
        ret->type = PARSER_ASTNODETYPE_NAMESPACE;
        ret->type = PARSER_ASTNODETYPE_NAMESPACE;
        end = Parser_parse_namespace(ret, scope, toks, start, allocs, diags);
    } else if (toks[start].type == LEXER_TOKENTYPE_RETURN) {
        printf("RETURN NODE\n");
        ret->type = PARSER_ASTNODETYPE_RETURN;
        end = Parser_parse_return(toks, start, ret, scope, allocs, diags);
    } else if (Parser_valid_type_start(toks, start, scope)) {
        printf("DECL NODE\n");
        bool mvp;
        if (Parser_decl_is_func(toks, start, scope, allocs, diags, &mvp)) {
            printf("mvp = %d\n", mvp);
            ret->type = PARSER_ASTNODETYPE_FUNC_DECL;
            end = Parser_parse_func_decl(toks, start, ret, scope,
                                         flags.skip_def, allocs, diags);
            check_semi = !ret->func_decl.has_def;
        } else {
            ret->type = PARSER_ASTNODETYPE_VAR_DECL;
            end = Parser_parse_var_decl(toks, start, PARSER_VARDECL_ENDTYPES,
                                        ret, scope, true, false, flags.skip_def,
                                        allocs, diags);
        }
    } else {
        printf("EXPR NODE\n");
        ret->type = PARSER_ASTNODETYPE_EXPR;
        ret->expr = Parser_parse_expr(toks, start, PARSER_DEFAULT_ENDTYPES,
                                      &end, scope, diags);
    }

    if (check_semi && toks[end].type != LEXER_TOKENTYPE_SEMICOLON)
        gen_dynpush(diags,
                    Diag_expected_token_err("';'", &toks[start],
                                            ERRORTYPE_MISSING_SEMICOLON));
    else if (check_semi)
        ++end;

    if (out_end)
        *out_end = end;
    printf("AST END\n");
    return ret;
}
