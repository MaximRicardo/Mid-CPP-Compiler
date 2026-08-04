#include "parser/ast.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "parser/class.h"
#include "parser/decl.h"
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
#include <string.h>

#ifdef MIDPAR_DEBUG_LOG_NODES
#include <stdio.h>
#endif

void midpar_ASTNode_deinit(struct midpar_ASTNode *self)
{
    switch (self->type) {
    case MIDPAR_ASTNODETYPE_ROOT:
        midgen_dyndeinit(&self->root);
        break;

    case MIDPAR_ASTNODETYPE_EXPR:
        midpar_Expr_deinit(&self->expr);
        break;

    case MIDPAR_ASTNODETYPE_VAR_DECL:
        midpar_VarDecl_deinit(&self->var_decl);
        break;

    case MIDPAR_ASTNODETYPE_VAR_DECL_INST:
        midpar_VarDeclInst_deinit(&self->var_inst);
        break;

    case MIDPAR_ASTNODETYPE_FUNC_DECL:
        midpar_FuncDecl_deinit(&self->func_decl);
        break;

    case MIDPAR_ASTNODETYPE_ENUM:
        midpar_Enum_deinit(&self->enum_);
        break;

    case MIDPAR_ASTNODETYPE_CLASS:
        midpar_Class_deinit(&self->class_);
        break;

    case MIDPAR_ASTNODETYPE_NAMESPACE:
        midpar_Namespace_deinit(&self->nmspace);
        break;

    case MIDPAR_ASTNODETYPE_RETURN:
        break;

    case MIDPAR_ASTNODETYPE_TMPLT:
        midpar_Tmplt_deinit(&self->tmplt);
        break;

    case MIDPAR_ASTNODETYPE_TMPLT_PARAM:
        midpar_TmpltParam_deinit(&self->tmplt_param);
        break;
    }
}

void midpar_copy_node(struct midpar_ASTNode *dest,
                      const struct midpar_ASTNode *src,
                      struct midpar_ASTNode *dest_parent,
                      struct midsema_Scope *dest_scope,
                      struct midpar_Allocators *allocs)
{
    *dest = (struct midpar_ASTNode){
        .parent = dest_parent, .start = src->start, .type = src->type};

    switch (src->type) {
    case MIDPAR_ASTNODETYPE_ROOT:
        dest->root = midpar_copy_nodepvec(&src->root, dest, dest_scope, allocs);
        break;

    case MIDPAR_ASTNODETYPE_EXPR:
        dest->expr = midpar_copy_expr(&src->expr);
        break;

    case MIDPAR_ASTNODETYPE_VAR_DECL:
        midpar_copy_var_decl(&dest->var_decl, &src->var_decl, dest_scope,
                             allocs);
        break;

    case MIDPAR_ASTNODETYPE_VAR_DECL_INST:
        midpar_copy_var_decl_inst(&dest->var_inst, &src->var_inst, dest_scope,
                                  allocs);
        break;

    case MIDPAR_ASTNODETYPE_FUNC_DECL:
        midpar_copy_func_decl(&dest->func_decl, &src->func_decl, dest_scope,
                              allocs);
        break;

    case MIDPAR_ASTNODETYPE_CLASS:
        midpar_copy_class(&dest->class_, &src->class_, dest_scope, allocs);
        break;

    case MIDPAR_ASTNODETYPE_ENUM:
        midpar_copy_enum(&dest->enum_, &src->enum_, dest_scope, allocs);
        break;

    case MIDPAR_ASTNODETYPE_NAMESPACE:
        midpar_copy_namespace(&dest->nmspace, &src->nmspace, dest_scope,
                              allocs);
        break;

    case MIDPAR_ASTNODETYPE_RETURN:
        midpar_copy_return(&dest->ret, &src->ret, allocs);
        break;

    case MIDPAR_ASTNODETYPE_TMPLT:
        midpar_copy_tmplt(&dest->tmplt, &src->tmplt, dest_scope, allocs);
        break;

    case MIDPAR_ASTNODETYPE_TMPLT_PARAM:
        midpar_copy_tmplt_param(&dest->tmplt_param, &src->tmplt_param, allocs);
        break;
    }
}

static bool is_class_start(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_CLASS || type == MIDLEX_TOKENTYPE_STRUCT ||
           type == MIDLEX_TOKENTYPE_UNION;
}

static mid_isize skip_typequals(const struct midlex_Token *toks,
                                mid_isize start)
{
    mid_isize i = start;
    while (midlex_is_typequal(toks[i++].type))
        ;

    return i - 1;
}

static bool is_ctor_start(const struct midlex_Token *toks, mid_isize start,
                          const struct midpar_ASTNode *parent)
{
    if (toks[start].type != MIDLEX_TOKENTYPE_IDENTIFIER)
        return false;
    if (toks[start + 1].type != MIDLEX_TOKENTYPE_L_PAREN)
        return false;

    assert(parent->type == MIDPAR_ASTNODETYPE_CLASS);
    return !strcmp(toks[start].ident, parent->class_.name);
}

static bool is_dtor_start(const struct midlex_Token *tok)
{
    return tok->type == MIDLEX_TOKENTYPE_BITWISE_NOT; // '~'
}

struct midpar_ASTNode *midpar_parse_node(const struct midlex_Token *toks,
                                         mid_isize start, mid_isize *out_end,
                                         struct midpar_ASTNode *parent,
                                         struct midsema_Scope *scope,
                                         struct midpar_ParseNodeFlags flags,
                                         struct midpar_Allocators *allocs,
                                         struct mid_DiagVec *diags)
{
#ifdef MIDPAR_DEBUG_LOG_NODES
#define LOG_NODE(x) x
#else
#define LOG_NODE(x)
#endif

    struct midpar_ASTNode *ret;
    midgen_bumpmalloc(&allocs->ast, &ret);
    *ret = (struct midpar_ASTNode){.start = &toks[start], .parent = parent};

    LOG_NODE(printf("AST START AT %d:%d\n", ret->start->pos.line,
                    ret->start->pos.column));

    mid_isize check_type = skip_typequals(toks, start);
    mid_isize end;
    bool check_semi = true;
    if (is_class_start(toks[check_type].type)) {
        LOG_NODE(printf("CLASS NODE\n"));
        ret->type = MIDPAR_ASTNODETYPE_CLASS;
        end = midpar_parse_class(&ret->class_, scope, toks, start,
                                 flags.skip_def, allocs, diags);
    } else if (flags.is_field && is_ctor_start(toks, check_type, parent)) {
        LOG_NODE(printf("CTOR NODE\n"));
        ret->type = MIDPAR_ASTNODETYPE_FUNC_DECL;
        end = midpar_parse_tor(&ret->func_decl, toks, start, scope,
                               flags.skip_def, allocs, diags);
        check_semi = !ret->func_decl.has_def;
    } else if (flags.is_field && is_dtor_start(&toks[check_type])) {
        LOG_NODE(printf("DTOR NODE\n"));
        ret->type = MIDPAR_ASTNODETYPE_FUNC_DECL;
        end = midpar_parse_tor(&ret->func_decl, toks, start, scope,
                               flags.skip_def, allocs, diags);
        check_semi = !ret->func_decl.has_def;
    } else if (toks[check_type].type == MIDLEX_TOKENTYPE_NAMESPACE) {
        LOG_NODE(printf("NAMESPACE NODE\n"));
        check_semi = false;
        ret->type = MIDPAR_ASTNODETYPE_NAMESPACE;
        end = midpar_parse_namespace(&ret->nmspace, scope, toks, start, allocs,
                                     diags);
    } else if (toks[start].type == MIDLEX_TOKENTYPE_RETURN) {
        LOG_NODE(printf("RETURN NODE\n"));
        ret->type = MIDPAR_ASTNODETYPE_RETURN;
        end = midpar_parse_return(&ret->ret, toks, start, scope, allocs, diags);
    } else if (toks[start].type == MIDLEX_TOKENTYPE_TEMPLATE) {
        LOG_NODE(printf("TEMPLATE NODE\n"));
        check_semi = false;
        ret->type = MIDPAR_ASTNODETYPE_TMPLT;
        end =
            midpar_parse_tmplt(&ret->tmplt, scope, toks, start, allocs, diags);
    } else if (midpar_valid_type_start(toks, start, scope)) {
        LOG_NODE(printf("DECL NODE\n"));
        bool mvp;
        bool is_func =
            midpar_decl_is_func(toks, start, scope, allocs, diags, &mvp);
        if (is_func) {
            LOG_NODE(printf("mvp = %d\n", mvp));
            ret->type = MIDPAR_ASTNODETYPE_FUNC_DECL;
            end = midpar_parse_func_decl(&ret->func_decl, toks, start, scope,
                                         flags.skip_def, allocs, diags);
            check_semi = !ret->func_decl.has_def;
        } else {
            ret->type = MIDPAR_ASTNODETYPE_VAR_DECL;
            end = midpar_parse_var_decl(
                &ret->var_decl, toks, start, MIDPAR_VARDECL_ENDTYPES,
                (struct midpar_ParseVarDeclFlags){.add_to_scope = true,
                                                  .skip_init = flags.skip_def},
                scope, allocs, diags);
        }
    } else {
        LOG_NODE(printf("EXPR NODE\n"));
        ret->type = MIDPAR_ASTNODETYPE_EXPR;
        ret->expr = midpar_parse_expr(toks, start, MIDPAR_DEFAULT_ENDTYPES,
                                      &end, scope, diags);
    }

    if (check_semi && toks[end].type != MIDLEX_TOKENTYPE_SEMICOLON)
        midgen_dynpush(
            diags, middiag_expected_token_err("';'", &toks[start],
                                              MIDDIAG_ERR_MISSING_SEMICOLON));
    else if (check_semi)
        ++end;

    if (out_end)
        *out_end = end;
    LOG_NODE(printf("AST END\n"));
    return ret;

#undef LOG_NODE
}

bool midpar_node_is_templated(const struct midpar_ASTNode *node)
{
    return node->parent && node->parent->type == MIDPAR_ASTNODETYPE_TMPLT;
}
