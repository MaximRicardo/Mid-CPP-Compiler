#include "parser/template.h"
#include "cmd.h"
#include "diag.h"
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
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <string.h>

static midlex_TokenIter
parse_tmplt_impl(struct midpar_Tmplt *self, struct midsema_Scope *parent_scope,
                 midlex_TokenIter start, struct midpar_Allocators *allocs,
                 bool is_param, struct mid_DiagVec *diags);

struct midpar_TmpltArg midpar_copy_tmplt_arg(struct midpar_TmpltArg *src)
{
    struct midpar_TmpltArg ret = {.kind = src->kind};

    switch (src->kind) {
    case MIDPAR_TMPLTARG_NONTYPE:
        ret.non_type = midpar_copy_expr(&src->non_type);
        break;

    case MIDPAR_TMPLTARG_TYPE:
        ret.type = midpar_copy_type(&src->type);
        break;

    case MIDPAR_TMPLTARG_TMPLT:
        ret.tmplt = src->tmplt;
        break;
    }

    return ret;
}

struct midpar_TmpltArgVec
midpar_copy_tmplt_argvec(const struct midpar_TmpltArgVec *src)
{
    struct midpar_TmpltArgVec ret = {};
    midgen_dynreserve(&ret, src->len);

    for (mid_isize i = 0; i < src->len; ++i) {
        midgen_dynpush(&ret, midpar_copy_tmplt_arg(&src->arr[i]));
    }

    return ret;
}

void midpar_TmpltInst_deinit(struct midpar_TmpltInst *self)
{
    midgen_dyndeinit(&self->args, midpar_TmpltArg_deinit);
}

void midpar_Tmplt_deinit(struct midpar_Tmplt *self)
{
    midgen_dyndeinit(&self->insts, midpar_TmpltInst_deinit);
    midgen_dyndeinit(&self->params);
}

void midpar_copy_tmplt(struct midpar_Tmplt *dest,
                       const struct midpar_Tmplt *src,
                       struct midsema_Scope *dest_scope,
                       struct midpar_Allocators *allocs)
{
    *dest = *src;

    midgen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope = midsema_create_empty_scope(
        MIDSEMA_SCOPETYPE_TEMPLATE, dest_scope, MIDPAR_GET_NODE(dest));

    auto params_nodes =
        midpar_copy_nodepvec((const struct midpar_ASTNodePVec *)&src->params,
                             MIDPAR_GET_NODE(dest), dest->scope, allocs);
    dest->params =
        (struct midpar_TmpltParamPVec){.arr = (void *)params_nodes.arr,
                                       .len = params_nodes.len,
                                       .cap = params_nodes.cap};

    midgen_bumpmalloc(&allocs->ast, &dest->child);
    midpar_copy_node(dest->child, src->child, MIDPAR_GET_NODE(dest),
                     dest->scope, allocs);
}

void midpar_TmpltNonTypeParam_deinit(struct midpar_TmpltNonTypeParam *self)
{
    midpar_Type_deinit(&self->type);
}

void midpar_TmpltTypeParam_deinit(struct midpar_TmpltTypeParam *self)
{
    if (self->def_arg) {
        midpar_Type_deinit(self->def_arg);
        free(self->def_arg);
    }
}

void midpar_TmpltTmpltParam_deinit(struct midpar_TmpltTmpltParam *self)
{
    (void)self;
}

void midpar_TmpltParam_deinit(struct midpar_TmpltParam *self)
{
    switch (self->kind) {
    case MIDPAR_TMPLTPARAM_NONTYPE:
        midpar_TmpltNonTypeParam_deinit(&self->non_type);
        break;

    case MIDPAR_TMPLTPARAM_TYPE:
        midpar_TmpltTypeParam_deinit(&self->type);
        break;

    case MIDPAR_TMPLTPARAM_TMPLT:
        midpar_TmpltTmpltParam_deinit(&self->tmplt);
        break;
    }
}

static struct midsema_Scope *
get_tmplt_scope(const struct midpar_TmpltParam *param)
{
    return MIDPAR_GET_PARENT(param)->tmplt.scope;
}

static const struct midsema_Ident *
tmplt_param_ident(const struct midpar_TmpltParam *param)
{
    auto scope = get_tmplt_scope(param);

    mid_isize idx;
    switch (param->kind) {
    case MIDPAR_TMPLTPARAM_NONTYPE:
        idx = param->non_type.ident_idx;
        break;

    case MIDPAR_TMPLTPARAM_TYPE:
        idx = param->type.ident_idx;
        break;

    case MIDPAR_TMPLTPARAM_TMPLT:
        idx = param->tmplt.ident_idx;
        break;
    }

    return &scope->idents.arr[idx];
}

static void set_ident_idx(struct midpar_TmpltParam *param, mid_isize ident_idx)
{
    switch (param->kind) {
    case MIDPAR_TMPLTPARAM_NONTYPE:
        param->non_type.ident_idx = ident_idx;
        break;

    case MIDPAR_TMPLTPARAM_TYPE:
        param->type.ident_idx = ident_idx;
        break;

    case MIDPAR_TMPLTPARAM_TMPLT:
        param->tmplt.ident_idx = ident_idx;
        break;
    }
}

static void copy_tmplt_nontype_param(struct midpar_TmpltNonTypeParam *dest,
                                     const struct midpar_TmpltNonTypeParam *src,
                                     struct midpar_Allocators *allocs)
{
    auto ident_idx = dest->ident_idx;
    *dest = *src;
    dest->ident_idx = ident_idx;

    dest->type = midpar_copy_type(&src->type);

    if (src->def_arg) {
        midgen_bumpmalloc(&allocs->expr, &dest->def_arg);
        *dest->def_arg = midpar_copy_expr(src->def_arg);
    }
}

static void copy_tmplt_type_param(struct midpar_TmpltTypeParam *dest,
                                  const struct midpar_TmpltTypeParam *src)
{
    if (src->def_arg) {
        dest->def_arg = mid_malloc(sizeof(*dest->def_arg));
        *dest->def_arg = midpar_copy_type(src->def_arg);
    }
}

static void copy_tmplt_tmplt_param(struct midpar_TmpltTmpltParam *dest,
                                   const struct midpar_TmpltTmpltParam *src,
                                   struct midpar_Allocators *allocs)
{
    midgen_bumpmalloc(&allocs->ast, (void **)&dest->tmplt);
    midpar_copy_node(MIDPAR_GET_NODE(dest->tmplt), MIDPAR_GET_NODE(src->tmplt),
                     (struct midpar_ASTNode *)dest,
                     get_tmplt_scope((const struct midpar_TmpltParam *)src),
                     allocs);
}

void midpar_copy_tmplt_param(struct midpar_TmpltParam *dest,
                             const struct midpar_TmpltParam *src,
                             struct midpar_Allocators *allocs)
{
    *dest = *src;

    auto scope = get_tmplt_scope(dest);
    auto old_ident =
        midsema_add_ident_copy(scope, tmplt_param_ident(src), false, allocs);
    if (old_ident)
        set_ident_idx(dest, old_ident - scope->idents.arr);
    else
        set_ident_idx(dest, scope->idents.len - 1);

    switch (dest->kind) {
    case MIDPAR_TMPLTPARAM_NONTYPE:
        copy_tmplt_nontype_param(&dest->non_type, &src->non_type, allocs);
        break;

    case MIDPAR_TMPLTPARAM_TYPE:
        copy_tmplt_type_param(&dest->type, &src->type);
        break;

    case MIDPAR_TMPLTPARAM_TMPLT:
        copy_tmplt_tmplt_param(&dest->tmplt, &src->tmplt, allocs);
        break;
    }
}

static struct midsema_Ident *add_ident(const char *name,
                                       struct midpar_ASTNode *node,
                                       struct midsema_Scope *scope,
                                       enum midsema_IdentType type)
{
    auto old = midsema_add_ident(
        scope, &(struct midsema_Ident){
                   .name = name, .decl = node, .def = NULL, .type = type});

    if (node->type == MIDPAR_ASTNODETYPE_TMPLT_PARAM) {
        if (old)
            set_ident_idx(&node->tmplt_param, midsema_ident_idx(old));
        else
            set_ident_idx(&node->tmplt_param, scope->idents.len - 1);
    }

    return old;
}

static void parse_tmplt_nontype_param(struct midpar_TmpltNonTypeParam *self,
                                      struct midsema_Scope *scope,
                                      midlex_TokenIter start,
                                      midlex_TokenIter *out_end,
                                      struct midpar_Allocators *allocs,
                                      struct mid_DiagVec *diags)
{
    *self = (struct midpar_TmpltNonTypeParam){};

    midlex_TokenIter name_idx;
    midlex_TokenIter type_end;
    self->type = midpar_parse_type(start, &type_end, scope, &name_idx, false,
                                   allocs, diags);

    if (name_idx) {
        self->name = name_idx->ident;
        if (add_ident(self->name, MIDPAR_GET_NODE(self), scope,
                      MIDSEMA_IDENTTYPE_VAR))
            midgen_dynpush(
                diags, middiag_ident_redefined_err(self->name, name_idx,
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    // default argument
    if (type_end->type == MIDLEX_TOKENTYPE_ASSIGN) {
        midlex_TokenIter expr_start = type_end + 1;
        midgen_bumpmalloc(&allocs->expr, &self->def_arg);
        *self->def_arg = midpar_parse_expr(
            expr_start, MIDPAR_TMPLT_PARAM_ENDTYPES, out_end, scope, diags);
    } else if (out_end) {
        *out_end = type_end;
    }
}

static void parse_tmplt_type_param(struct midpar_TmpltTypeParam *self,
                                   struct midsema_Scope *scope,
                                   midlex_TokenIter start,
                                   midlex_TokenIter *out_end,
                                   struct midpar_Allocators *allocs,
                                   struct mid_DiagVec *diags)
{
    *self = (struct midpar_TmpltTypeParam){};

    midlex_TokenIter name_idx = start + 1;
    midlex_TokenIter assign_idx = name_idx;
    if (name_idx->type == MIDLEX_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        self->name = name_idx->ident;
        if (add_ident(self->name, MIDPAR_GET_NODE(self), scope,
                      MIDSEMA_IDENTTYPE_TYPEDEF))
            midgen_dynpush(
                diags, middiag_ident_redefined_err(self->name, name_idx,
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    // default argument
    if (assign_idx->type == MIDLEX_TOKENTYPE_ASSIGN) {
        midlex_TokenIter type_start = assign_idx + 1;
        midlex_TokenIter def_name_idx;
        self->def_arg = mid_malloc(sizeof(*self->def_arg));
        *self->def_arg = midpar_parse_type(type_start, out_end, scope,
                                           &def_name_idx, true, allocs, diags);
    } else if (out_end) {
        *out_end = assign_idx;
    }
}

static void parse_tmplt_tmplt_param(struct midpar_TmpltTmpltParam *self,
                                    struct midsema_Scope *scope,
                                    midlex_TokenIter start,
                                    midlex_TokenIter *out_end,
                                    struct midpar_Allocators *allocs,
                                    struct mid_DiagVec *diags)
{
    *self = (struct midpar_TmpltTmpltParam){};

    midgen_bumpmalloc(&allocs->ast, (void **)&self->tmplt);
    MIDPAR_GET_PARENT(self->tmplt) = MIDPAR_GET_NODE(self);
    MIDPAR_GET_START(self->tmplt) = start;
    MIDPAR_GET_TYPE(self->tmplt) = MIDPAR_ASTNODETYPE_TMPLT;

    midlex_TokenIter name_idx =
        parse_tmplt_impl(self->tmplt, scope, start, allocs, true, diags);

    midlex_TokenIter assign_idx = name_idx;
    if (name_idx->type == MIDLEX_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        self->name = name_idx->ident;
        if (add_ident(self->name, MIDPAR_GET_NODE(self), scope,
                      MIDSEMA_IDENTTYPE_TMPLT_CLASS))
            midgen_dynpush(
                diags, middiag_ident_redefined_err(self->name, name_idx,
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    // no default argument
    if (assign_idx->type != MIDLEX_TOKENTYPE_ASSIGN) {
        if (out_end)
            *out_end = assign_idx;
        return;
    }

    MID_CRASH(
        "default argument for template template parameter not yet supported");
}

void parse_tmplt_param(struct midpar_TmpltParam *self,
                       struct midsema_Scope *scope, midlex_TokenIter start,
                       midlex_TokenIter *out_end,
                       struct midpar_Allocators *allocs,
                       struct mid_DiagVec *diags)
{
    MIDPAR_GET_START(self) = start;
    MIDPAR_GET_TYPE(self) = MIDPAR_ASTNODETYPE_TMPLT_PARAM;

    if (start->type == MIDLEX_TOKENTYPE_TEMPLATE) {
        self->kind = MIDPAR_TMPLTPARAM_TMPLT;
        parse_tmplt_tmplt_param(&self->tmplt, scope, start, out_end, allocs,
                                diags);
    } else if (start->type == MIDLEX_TOKENTYPE_CLASS ||
               start->type == MIDLEX_TOKENTYPE_TYPENAME) {
        self->kind = MIDPAR_TMPLTPARAM_TYPE;
        parse_tmplt_type_param(&self->type, scope, start, out_end, allocs,
                               diags);
    } else {
        self->kind = MIDPAR_TMPLTPARAM_NONTYPE;
        parse_tmplt_nontype_param(&self->non_type, scope, start, out_end,
                                  allocs, diags);
    }
}

static struct midpar_TmpltParamPVec parse_tmplt_param_list(
    struct midpar_ASTNode *parent, struct midsema_Scope *scope,
    midlex_TokenIter l_angle, midlex_TokenIter *out_r_angle,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    struct midpar_TmpltParamPVec params = {};

    midlex_TokenIter r_angle = midpar_find_twin_angle(l_angle, nullptr);
    if (out_r_angle)
        *out_r_angle = !r_angle ? l_angle : r_angle;

    if (!r_angle) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  ">", l_angle, MIDDIAG_ERR_MISSING_ANGLE));
        return params;
    }

    for (midlex_TokenIter i = l_angle + 1; i < r_angle; ++i) {
        struct midpar_TmpltParam *param;
        midgen_bumpmalloc(&allocs->ast, (void **)&param);

        MIDPAR_GET_PARENT(param) = parent;
        parse_tmplt_param(param, scope, i, &i, allocs, diags);

        midgen_dynpush(&params, param);
    }

    return params;
}

static struct midsema_Scope *create_scope(struct midsema_Scope *parent,
                                          struct midpar_Tmplt *self,
                                          struct midpar_Allocators *allocs)
{
    struct midsema_Scope *ret;
    midgen_bumpmalloc(&allocs->scope, &ret);
    *ret = (struct midsema_Scope){.parent = parent,
                                  .node = MIDPAR_GET_NODE(self),
                                  .type = MIDSEMA_SCOPETYPE_TEMPLATE};
    midgen_dynpush(&parent->childs, ret);

    return ret;
}

// is_param     - is the template a template template parameter? if so the
//                function stops after the type parameter key
static midlex_TokenIter
parse_tmplt_impl(struct midpar_Tmplt *self, struct midsema_Scope *parent_scope,
                 midlex_TokenIter start, struct midpar_Allocators *allocs,
                 bool is_param, struct mid_DiagVec *diags)
{
    assert(start->type == MIDLEX_TOKENTYPE_TEMPLATE);

    *self = (struct midpar_Tmplt){};

    midlex_TokenIter l_angle = start + 1;
    if (l_angle->type != MIDLEX_TTALIAS_L_ANGLE) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "<", start, MIDDIAG_ERR_MISSING_ANGLE));
        return start;
    }

    self->scope = create_scope(parent_scope, self, allocs);

    midlex_TokenIter r_angle;
    self->params = parse_tmplt_param_list(MIDPAR_GET_NODE(self), self->scope,
                                          l_angle, &r_angle, allocs, diags);

    if (is_param) {
        if ((r_angle + 1)->type != MIDLEX_TOKENTYPE_CLASS) {
            midgen_dynpush(
                diags, middiag_expected_token_err("class", r_angle + 1,
                                                  MIDDIAG_ERR_MISSING_TOKEN));
            return r_angle + 1;
        } else {
            return r_angle + 2;
        }
    } else {
        return r_angle + 1;
    }
}

static void check_child_valid(const struct midpar_Tmplt *tmplt,
                              struct mid_DiagVec *diags)
{
    if (tmplt->child->type != MIDPAR_ASTNODETYPE_FUNC_DECL &&
        tmplt->child->type != MIDPAR_ASTNODETYPE_CLASS) {
        midgen_dynpush(
            diags, ((struct mid_Diag){.pos = tmplt->child->start->pos,
                                      .line = tmplt->child->start->line,
                                      .msg = midcmd_fmt_to_str(
                                          "statement can not be a template"),
                                      .err = MIDDIAG_ERR_BAD_TEMPLATE,
                                      .type = MIDDIAG_TYPE_ERROR}));
    }
}

midlex_TokenIter midpar_parse_tmplt(struct midpar_Tmplt *self,
                                    struct midsema_Scope *parent_scope,
                                    midlex_TokenIter start,
                                    struct midpar_Allocators *allocs,
                                    struct mid_DiagVec *diags)
{
    midlex_TokenIter child_start =
        parse_tmplt_impl(self, parent_scope, start, allocs, false, diags);

    midlex_TokenIter child_end;
    self->child = midpar_parse_node(
        child_start, &child_end, MIDPAR_GET_NODE(self), self->scope,
        (struct midpar_ParseNodeFlags){}, allocs, diags);

    check_child_valid(self, diags);

    return child_end;
}

void midpar_TmpltArg_deinit(struct midpar_TmpltArg *self)
{
    switch (self->kind) {
    case MIDPAR_TMPLTARG_NONTYPE:
        midpar_Expr_deinit(&self->non_type);
        break;

    case MIDPAR_TMPLTARG_TYPE:
        midpar_Type_deinit(&self->type);
        break;

    case MIDPAR_TMPLTARG_TMPLT:
        break;
    }
}

static bool is_tmplt_tmplt_arg(const struct midlex_Token *tok,
                               struct midsema_Scope *scope,
                               struct midsema_Ident **out_ident)
{
    if (tok->type != MIDLEX_TOKENTYPE_IDENTIFIER)
        return false;

    auto ident = midsema_find_ident(scope, tok->ident);
    if (out_ident)
        *out_ident = ident;
    if (!ident)
        return false;

    return midsema_ident_is_tmplt(ident->type);
}

static struct midpar_TmpltArg parse_tmplt_arg(midlex_TokenIter start,
                                              midlex_TokenIter *out_end,
                                              struct midsema_Scope *scope,
                                              struct midpar_Allocators *allocs,
                                              struct mid_DiagVec *diags)
{
    struct midpar_TmpltArg arg = {};

    struct midsema_Ident *ident;
    if (is_tmplt_tmplt_arg(start, scope, &ident)) {
        arg.kind = MIDPAR_TMPLTARG_TMPLT;
        arg.tmplt = midsema_create_identptr(ident);
    } else if (midpar_valid_type_start(start, scope)) {
        arg.kind = MIDPAR_TMPLTARG_TYPE;
        arg.type =
            midpar_parse_type(start, out_end, scope, NULL, true, allocs, diags);
    } else {
        arg.kind = MIDPAR_TMPLTARG_NONTYPE;
        arg.non_type = midpar_parse_expr(start, MIDPAR_TMPLT_ARG_ENDTYPES,
                                         out_end, scope, diags);
    }

    return arg;
}

struct midpar_TmpltArgVec
midpar_parse_tmplt_args(midlex_TokenIter l_angle, midlex_TokenIter *out_r_angle,
                        struct midsema_Scope *scope,
                        struct midpar_Allocators *allocs,
                        struct mid_DiagVec *diags)
{
    struct midpar_TmpltArgVec args = {};

    if (l_angle->type != MIDLEX_TTALIAS_L_ANGLE) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "'<'", l_angle, MIDDIAG_ERR_MISSING_ANGLE));
        if (out_r_angle)
            *out_r_angle = l_angle;
        return args;
    }

    midlex_TokenIter r_angle = midpar_find_twin_angle(l_angle, nullptr);
    if (!r_angle) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "'<'", l_angle, MIDDIAG_ERR_MISSING_ANGLE));
        if (out_r_angle)
            *out_r_angle = l_angle;
        return args;
    }

    if (out_r_angle)
        *out_r_angle = r_angle;

    for (midlex_TokenIter i = l_angle + 1; i < r_angle; ++i) {
        auto arg = parse_tmplt_arg(i, &i, scope, allocs, diags);
        if (i < r_angle && i->type != MIDLEX_TOKENTYPE_COMMA)
            midgen_dynpush(diags, middiag_expected_token_err(
                                      "','", i, MIDDIAG_ERR_MISSING_COMMA));

        midgen_dynpush(&args, arg);
    }

    return args;
}
