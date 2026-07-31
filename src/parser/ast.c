#include "ast.h"
#include "allocator.h"
#include "decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/astvec.h"
#include "parser/class.h"
#include "parser/end_types.h"
#include "parser/enum.h"
#include "parser/expr.h"
#include "parser/func_decl.h"
#include "parser/namespace.h"
#include "parser/return.h"
#include "parser/template.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/scope.h"
#include <stdio.h>
#include <string.h>

void MidParser_ASTNode_deinit(struct MidParser_ASTNode *self)
{
    switch (self->type) {
    case MIDPARSER_ASTNODETYPE_ROOT:
        MidGen_dyndeinit(&self->root);
        break;

    case MIDPARSER_ASTNODETYPE_EXPR:
        MidParser_Expr_deinit(&self->expr);
        break;

    case MIDPARSER_ASTNODETYPE_VAR_DECL:
        MidParser_VarDecl_deinit(&self->var_decl);
        break;

    case MIDPARSER_ASTNODETYPE_VAR_DECL_INST:
        MidParser_VarDeclInst_deinit(&self->var_inst);
        break;

    case MIDPARSER_ASTNODETYPE_FUNC_DECL:
        MidParser_FuncDecl_deinit(&self->func_decl);
        break;

    case MIDPARSER_ASTNODETYPE_ENUM:
        MidParser_Enum_deinit(&self->enum_);
        break;

    case MIDPARSER_ASTNODETYPE_CLASS:
        MidParser_Class_deinit(&self->class_);
        break;

    case MIDPARSER_ASTNODETYPE_NAMESPACE:
        MidParser_Namespace_deinit(&self->nmspace);
        break;

    case MIDPARSER_ASTNODETYPE_RETURN:
        break;

    case MIDPARSER_ASTNODETYPE_TMPLT:
        MidParser_Tmplt_deinit(&self->tmplt);
        break;

    case MIDPARSER_ASTNODETYPE_TMPLT_PARAM:
        MidParser_TmpltParam_deinit(&self->tmplt_param);
        break;

    default:
        MID_CRASH("invalid node type");
    }
}

void MidParser_copy_node(struct MidParser_ASTNode *dest,
                      const struct MidParser_ASTNode *src,
                      struct MidParser_ASTNode *dest_parent,
                      struct MidSema_Scope *dest_scope,
                      struct MidParser_Allocators *allocs)
{
    *dest = (struct MidParser_ASTNode){
        .parent = dest_parent, .start = src->start, .type = src->type};

    switch (src->type) {
    case MIDPARSER_ASTNODETYPE_ROOT:
        dest->root = MidParser_copy_nodepvec(&src->root, dest, dest_scope, allocs);
        break;

    case MIDPARSER_ASTNODETYPE_EXPR:
        dest->expr = MidParser_copy_expr(&src->expr);
        break;

    case MIDPARSER_ASTNODETYPE_VAR_DECL:
        MidParser_copy_var_decl(&dest->var_decl, &src->var_decl, dest_scope,
                             allocs);
        break;

    case MIDPARSER_ASTNODETYPE_VAR_DECL_INST:
        MidParser_copy_var_decl_inst(&dest->var_inst, &src->var_inst, dest_scope,
                                  allocs);
        break;

    case MIDPARSER_ASTNODETYPE_FUNC_DECL:
        MidParser_copy_func_decl(&dest->func_decl, &src->func_decl, dest_scope,
                              allocs);
        break;

    case MIDPARSER_ASTNODETYPE_CLASS:
        MidParser_copy_class(&dest->class_, &src->class_, dest_scope, allocs);
        break;

    case MIDPARSER_ASTNODETYPE_ENUM:
        MidParser_copy_enum(&dest->enum_, &src->enum_, dest_scope, allocs);
        break;

    case MIDPARSER_ASTNODETYPE_NAMESPACE:
        MidParser_copy_namespace(&dest->nmspace, &src->nmspace, dest_scope,
                              allocs);
        break;

    case MIDPARSER_ASTNODETYPE_RETURN:
        MidParser_copy_return(&dest->ret, &src->ret, allocs);
        break;

    case MIDPARSER_ASTNODETYPE_TMPLT:
        MidParser_copy_tmplt(&dest->tmplt, &src->tmplt, dest_scope, allocs);
        break;

    case MIDPARSER_ASTNODETYPE_TMPLT_PARAM:
        MidParser_copy_tmplt_param(&dest->tmplt_param, &src->tmplt_param, allocs);
        break;

    default:
        MID_CRASH("invalid ast node type");
    }
}

static bool is_class_start(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_CLASS || type == MIDLEXER_TOKENTYPE_STRUCT ||
           type == MIDLEXER_TOKENTYPE_UNION;
}

static mid_isize skip_typequals(const struct MidLexer_Token *toks, mid_isize start)
{
    mid_isize i = start;
    while (MidLexer_is_typequal(toks[i++].type))
        ;

    return i - 1;
}

static bool is_ctor_start(const struct MidLexer_Token *toks, mid_isize start,
                          const struct MidParser_ASTNode *parent)
{
    if (toks[start].type != MIDLEXER_TOKENTYPE_IDENTIFIER)
        return false;
    if (toks[start + 1].type != MIDLEXER_TOKENTYPE_L_PAREN)
        return false;

    assert(parent->type == MIDPARSER_ASTNODETYPE_CLASS);
    return !strcmp(toks[start].ident, parent->class_.name);
}

static bool is_dtor_start(const struct MidLexer_Token *tok)
{
    return tok->type == MIDLEXER_TOKENTYPE_BITWISE_NOT; // '~'
}

struct MidParser_ASTNode *
MidParser_parse_node(const struct MidLexer_Token *toks, mid_isize start,
                  mid_isize *out_end, struct MidParser_ASTNode *parent,
                  struct MidSema_Scope *scope, struct MidParser_ParseNodeFlags flags,
                  struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    struct MidParser_ASTNode *ret;
    MidGen_bumpmalloc(&allocs->ast, &ret);
    *ret = (struct MidParser_ASTNode){.start = &toks[start], .parent = parent};

    printf("AST START AT %d:%d\n", ret->start->pos.line,
           ret->start->pos.column);

    mid_isize check_type = skip_typequals(toks, start);
    mid_isize end;
    bool check_semi = true;
    if (is_class_start(toks[check_type].type)) {
        printf("CLASS NODE\n");
        ret->type = MIDPARSER_ASTNODETYPE_CLASS;
        end = MidParser_parse_class(&ret->class_, scope, toks, start,
                                 flags.skip_def, allocs, diags);
    } else if (flags.is_field && is_ctor_start(toks, check_type, parent)) {
        printf("CTOR NODE\n");
        ret->type = MIDPARSER_ASTNODETYPE_FUNC_DECL;
        end = MidParser_parse_tor(&ret->func_decl, toks, start, scope,
                               flags.skip_def, allocs, diags);
        check_semi = !ret->func_decl.has_def;
    } else if (flags.is_field && is_dtor_start(&toks[check_type])) {
        printf("DTOR NODE\n");
        ret->type = MIDPARSER_ASTNODETYPE_FUNC_DECL;
        end = MidParser_parse_tor(&ret->func_decl, toks, start, scope,
                               flags.skip_def, allocs, diags);
        check_semi = !ret->func_decl.has_def;
    } else if (toks[check_type].type == MIDLEXER_TOKENTYPE_NAMESPACE) {
        printf("NAMESPACE NODE\n");
        check_semi = false;
        ret->type = MIDPARSER_ASTNODETYPE_NAMESPACE;
        end = MidParser_parse_namespace(&ret->nmspace, scope, toks, start, allocs,
                                     diags);
    } else if (toks[start].type == MIDLEXER_TOKENTYPE_RETURN) {
        printf("RETURN NODE\n");
        ret->type = MIDPARSER_ASTNODETYPE_RETURN;
        end = MidParser_parse_return(&ret->ret, toks, start, scope, allocs, diags);
    } else if (toks[start].type == MIDLEXER_TOKENTYPE_TEMPLATE) {
        printf("TEMPLATE NODE\n");
        check_semi = false;
        ret->type = MIDPARSER_ASTNODETYPE_TMPLT;
        end =
            MidParser_parse_tmplt(&ret->tmplt, scope, toks, start, allocs, diags);
    } else if (MidParser_valid_type_start(toks, start, scope)) {
        printf("DECL NODE\n");
        bool mvp;
        bool is_func =
            MidParser_decl_is_func(toks, start, scope, allocs, diags, &mvp);
        if (is_func) {
            printf("mvp = %d\n", mvp);
            ret->type = MIDPARSER_ASTNODETYPE_FUNC_DECL;
            end = MidParser_parse_func_decl(&ret->func_decl, toks, start, scope,
                                         flags.skip_def, allocs, diags);
            check_semi = !ret->func_decl.has_def;
        } else {
            ret->type = MIDPARSER_ASTNODETYPE_VAR_DECL;
            end = MidParser_parse_var_decl(
                &ret->var_decl, toks, start, MIDPARSER_VARDECL_ENDTYPES,
                (struct MidParser_ParseVarDeclFlags){.add_to_scope = true,
                                                  .skip_init = flags.skip_def},
                scope, allocs, diags);
        }
    } else {
        printf("EXPR NODE\n");
        ret->type = MIDPARSER_ASTNODETYPE_EXPR;
        ret->expr = MidParser_parse_expr(toks, start, MIDPARSER_DEFAULT_ENDTYPES,
                                      &end, scope, diags);
    }

    if (check_semi && toks[end].type != MIDLEXER_TOKENTYPE_SEMICOLON)
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("';'", &toks[start],
                                            MIDDIAG_ERR_MISSING_SEMICOLON));
    else if (check_semi)
        ++end;

    if (out_end)
        *out_end = end;
    printf("AST END\n");
    return ret;
}

bool MidParser_node_is_templated(const struct MidParser_ASTNode *node)
{
    return node->parent && node->parent->type == MIDPARSER_ASTNODETYPE_TMPLT;
}
