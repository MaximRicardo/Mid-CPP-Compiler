#include "template.h"
#include "diag.h"
#include "end_types.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/scope.h"

static isize_t parse_tmplt_impl(struct Parser_ASTNode *node,
                                struct Sema_Scope *parent_scope,
                                const struct Lexer_Token *toks, isize_t start,
                                struct Parser_Allocators *allocs, bool is_param,
                                struct DiagVec *diags);

void Parser_Tmplt_deinit(struct Parser_Tmplt *self)
{
    gen_dyndeinit(&self->params);
}

void Parser_TmpltNonTypeParam_deinit(struct Parser_TmpltNonTypeParam *self)
{
    Parser_Type_deinit(&self->type);
}

void Parser_TmpltTypeParam_deinit(struct Parser_TmpltTypeParam *self)
{
    if (self->def_arg) {
        Parser_Type_deinit(self->def_arg);
        free(self->def_arg);
    }
}

void Parser_TmpltTmpltParam_deinit(struct Parser_TmpltTmpltParam *self)
{
    (void)self;
}

void Parser_TmpltParam_deinit(struct Parser_TmpltParam *self)
{
    switch (self->kind) {
    case PARSER_TMPLTPARAM_NONTYPE:
        Parser_TmpltNonTypeParam_deinit(&self->non_type);
        break;

    case PARSER_TMPLTPARAM_TYPE:
        Parser_TmpltTypeParam_deinit(&self->type);
        break;

    case PARSER_TMPLTPARAM_TMPLT:
        Parser_TmpltTmpltParam_deinit(&self->tmplt);
        break;

    default:
        CRASH("invalid template param kind");
    }
}

static struct Sema_Ident *add_ident(const char *name,
                                    struct Parser_ASTNode *node,
                                    struct Sema_Scope *scope,
                                    enum Sema_IdentType type)
{
    return Sema_add_ident(
        scope, &(struct Sema_Ident){
                   .name = name, .decl = node, .def = NULL, .type = type});
}

static void parse_tmplt_nontype_param(
    struct Parser_ASTNode *node, struct Parser_ASTNode *parent,
    struct Sema_Scope *scope, const struct Lexer_Token *toks, isize_t start,
    isize_t *out_end, struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    auto param = &node->tmplt_param.non_type;
    *param = (struct Parser_TmpltNonTypeParam){.parent = parent};

    isize_t name_idx;
    isize_t type_end;
    param->type = Parser_parse_type(toks, start, &type_end, scope, &name_idx,
                                    false, diags);

    if (name_idx != -1) {
        param->name = toks[name_idx].ident;
        if (add_ident(param->name, node, scope, SEMA_IDENTTYPE_VAR))
            gen_dynpush(diags,
                        Diag_ident_redefined_err(param->name, &toks[name_idx],
                                                 ERRORTYPE_BAD_IDENTIFIER));
    }

    // default argument
    if (toks[type_end].type == LEXER_TOKENTYPE_ASSIGN) {
        isize_t expr_start = type_end + 1;
        gen_bumpmalloc(&allocs->expr, &param->def_arg);
        *param->def_arg =
            Parser_parse_expr(toks, expr_start, PARSER_TMPLT_PARAM_ENDTYPES,
                              out_end, scope, diags);
    } else if (out_end) {
        *out_end = type_end;
    }
}

static void parse_tmplt_type_param(struct Parser_ASTNode *node,
                                   struct Parser_ASTNode *parent,
                                   struct Sema_Scope *scope,
                                   const struct Lexer_Token *toks,
                                   isize_t start, isize_t *out_end,
                                   struct DiagVec *diags)
{
    auto param = &node->tmplt_param.type;
    *param = (struct Parser_TmpltTypeParam){.parent = parent};

    isize_t name_idx = start + 1;
    isize_t assign_idx = name_idx;
    if (toks[name_idx].type == LEXER_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        param->name = toks[name_idx].ident;
        if (add_ident(param->name, node, scope, SEMA_IDENTTYPE_TYPEDEF))
            gen_dynpush(diags,
                        Diag_ident_redefined_err(param->name, &toks[name_idx],
                                                 ERRORTYPE_BAD_IDENTIFIER));
    }

    // default argument
    if (toks[assign_idx].type == LEXER_TOKENTYPE_ASSIGN) {
        isize_t type_start = assign_idx + 1;
        isize_t def_name_idx;
        param->def_arg = mid_malloc(sizeof(*param->def_arg));
        *param->def_arg = Parser_parse_type(toks, type_start, out_end, scope,
                                            &def_name_idx, true, diags);
    } else if (out_end) {
        *out_end = assign_idx;
    }
}

static void parse_tmplt_tmplt_param(
    struct Parser_ASTNode *node, struct Parser_ASTNode *parent,
    struct Sema_Scope *scope, const struct Lexer_Token *toks, isize_t start,
    isize_t *out_end, struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    auto param = &node->tmplt_param.tmplt;
    *param = (struct Parser_TmpltTmpltParam){.parent = parent};

    gen_bumpmalloc(&allocs->ast, &param->tmplt);
    param->tmplt->parent = node;
    param->tmplt->start = &toks[start];
    param->tmplt->type = PARSER_ASTNODETYPE_TMPLT;

    isize_t name_idx =
        parse_tmplt_impl(param->tmplt, scope, toks, start, allocs, true, diags);

    isize_t assign_idx = name_idx;
    if (toks[name_idx].type == LEXER_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        param->name = toks[name_idx].ident;
        if (add_ident(param->name, node, scope, SEMA_IDENTTYPE_TEMPLATE))
            gen_dynpush(diags,
                        Diag_ident_redefined_err(param->name, &toks[name_idx],
                                                 ERRORTYPE_BAD_IDENTIFIER));
    }

    // no default argument
    if (toks[assign_idx].type != LEXER_TOKENTYPE_ASSIGN) {
        if (out_end)
            *out_end = assign_idx;
        return;
    }

    CRASH("default argument for template template parameter not yet supported");
}

void parse_tmplt_param(struct Parser_ASTNode *node,
                       struct Parser_ASTNode *parent, struct Sema_Scope *scope,
                       const struct Lexer_Token *toks, isize_t start,
                       isize_t *out_end, struct Parser_Allocators *allocs,
                       struct DiagVec *diags)
{
    node->start = &toks[start];
    node->type = PARSER_ASTNODETYPE_TMPLT_PARAM;
    auto param = &node->tmplt_param;

    if (toks[start].type == LEXER_TOKENTYPE_TEMPLATE) {
        param->kind = PARSER_TMPLTPARAM_TMPLT;
        parse_tmplt_tmplt_param(node, parent, scope, toks, start, out_end,
                                allocs, diags);
    } else if (toks[start].type == LEXER_TOKENTYPE_CLASS ||
               toks[start].type == LEXER_TOKENTYPE_TYPENAME) {
        param->kind = PARSER_TMPLTPARAM_TYPE;
        parse_tmplt_type_param(node, parent, scope, toks, start, out_end,
                               diags);
    } else {
        param->kind = PARSER_TMPLTPARAM_NONTYPE;
        parse_tmplt_nontype_param(node, parent, scope, toks, start, out_end,
                                  allocs, diags);
    }
}

static struct Parser_ASTNodePVec
parse_tmplt_param_list(struct Parser_ASTNode *parent, struct Sema_Scope *scope,
                       const struct Lexer_Token *toks, isize_t l_angle,
                       isize_t *out_r_angle, struct Parser_Allocators *allocs,
                       struct DiagVec *diags)
{
    struct Parser_ASTNodePVec params = {};

    isize_t r_angle = Parser_find_twin_angle(toks, l_angle, ISIZE_MAX);
    if (out_r_angle)
        *out_r_angle = r_angle == -1 ? l_angle : r_angle;

    if (r_angle == -1) {
        gen_dynpush(diags, Diag_expected_token_err(">", &toks[l_angle],
                                                   ERRORTYPE_MISSING_ANGLE));
        return params;
    }

    for (isize_t i = l_angle + 1; i < r_angle; ++i) {
        struct Parser_ASTNode *param;
        gen_bumpmalloc(&allocs->ast, &param);

        param->parent = parent;
        parse_tmplt_param(param, parent, scope, toks, i, &i, allocs, diags);

        gen_dynpush(&params, param);
    }

    return params;
}

static struct Sema_Scope *create_scope(struct Sema_Scope *parent,
                                       struct Parser_ASTNode *node,
                                       struct Parser_Allocators *allocs)
{
    struct Sema_Scope *ret;
    gen_bumpmalloc(&allocs->scope, &ret);
    *ret = (struct Sema_Scope){
        .parent = parent, .node = node, .type = SEMA_SCOPETYPE_TEMPLATE};
    gen_dynpush(&parent->childs, ret);

    return ret;
}

// is_param     - is the template a template template parameter? if so the
//                function stops after the type parameter key
static isize_t parse_tmplt_impl(struct Parser_ASTNode *node,
                                struct Sema_Scope *parent_scope,
                                const struct Lexer_Token *toks, isize_t start,
                                struct Parser_Allocators *allocs, bool is_param,
                                struct DiagVec *diags)
{
    assert(toks[start].type == LEXER_TOKENTYPE_TEMPLATE);

    auto tmplt = &node->tmplt;
    *tmplt = (struct Parser_Tmplt){};

    isize_t l_angle = start + 1;
    if (toks[l_angle].type != LEXER_TTALIAS_L_ANGLE) {
        gen_dynpush(diags, Diag_expected_token_err("<", &toks[start],
                                                   ERRORTYPE_MISSING_ANGLE));
        return start;
    }

    tmplt->scope = create_scope(parent_scope, node, allocs);

    isize_t r_angle;
    tmplt->params = parse_tmplt_param_list(node, tmplt->scope, toks, l_angle,
                                           &r_angle, allocs, diags);

    if (is_param) {
        if (toks[r_angle + 1].type != LEXER_TOKENTYPE_CLASS) {
            gen_dynpush(diags,
                        Diag_expected_token_err("class", &toks[r_angle + 1],
                                                ERRORTYPE_MISSING_TOKEN));
            return r_angle + 1;
        } else {
            return r_angle + 2;
        }
    } else {
        return r_angle + 1;
    }
}

isize_t Parser_parse_tmplt(struct Parser_ASTNode *node,
                           struct Sema_Scope *parent_scope,
                           const struct Lexer_Token *toks, isize_t start,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    isize_t child_start =
        parse_tmplt_impl(node, parent_scope, toks, start, allocs, false, diags);

    auto tmplt = &node->tmplt;

    isize_t child_end;
    tmplt->child =
        Parser_parse_node(toks, child_start, &child_end, node, tmplt->scope,
                          (struct Parser_ParseNodeFlags){}, allocs, diags);

    return child_end;
}
