#include "ast.h"
#include "allocator.h"
#include "decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/class.h"
#include "parser/end_types.h"
#include "parser/enum.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/scope.h"
#include <stdio.h>
#include <stdlib.h>

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
    }
}

static struct Diag missing_semi_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("missing semicolon"),
        .err = ERRORTYPE_MISSING_SEMICOLON,
        .is_err = true,
    };
}

static bool is_class_start(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_CLASS || type == LEXER_TOKENTYPE_STRUCT ||
           type == LEXER_TOKENTYPE_UNION;
}

struct Parser_ASTNode *
Parser_parse_node(const struct Lexer_Token *toks, isize_t start,
                  isize_t *out_end, struct Parser_ASTNode *parent,
                  struct Sema_Scope *scope, bool skip_def,
                  struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    struct Parser_ASTNode *ret;
    gen_bumpmalloc(&allocs->ast, &ret);
    *ret = (struct Parser_ASTNode){.start = &toks[start], .parent = parent};

    printf("AST START AT %d:%d\n", ret->start->pos.line,
           ret->start->pos.column);

    isize_t end;
    bool check_semi = true;
    if (is_class_start(toks[start].type)) {
        printf("CLASS NODE\n");
        ret->type = PARSER_ASTNODETYPE_CLASS;
        end = Parser_parse_class(&ret->class_, ret, scope, toks, start,
                                 skip_def, allocs, diags);
    } else if (Parser_valid_type_start(&toks[start], scope)) {
        printf("DECL NODE\n");
        bool mvp;
        if (Parser_decl_is_func(toks, start, scope, allocs, diags, &mvp)) {
            printf("mvp = %d\n", mvp);
            ret->type = PARSER_ASTNODETYPE_FUNC_DECL;
            end = Parser_parse_func_decl(toks, start, &ret->func_decl, ret,
                                         scope, skip_def, allocs, diags);
            check_semi = !ret->func_decl.has_def;
        } else {
            ret->type = PARSER_ASTNODETYPE_VAR_DECL;
            end = Parser_parse_var_decl(toks, start, PARSER_DEFAULT_ENDTYPES,
                                        &ret->var_decl, ret, scope, true,
                                        skip_def, allocs, diags);
        }
    } else {
        printf("EXPR NODE\n");
        ret->type = PARSER_ASTNODETYPE_EXPR;
        ret->expr = Parser_parse_expr(toks, start, PARSER_DEFAULT_ENDTYPES,
                                      &end, diags);
    }

    if (check_semi && toks[end].type != LEXER_TOKENTYPE_SEMICOLON)
        gen_dynpush(diags, missing_semi_err(&toks[start]));
    else if (check_semi)
        ++end;

    if (out_end)
        *out_end = end;
    return ret;
}
