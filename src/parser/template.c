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
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <string.h>

static mid_isize parse_tmplt_impl(struct MidParser_Tmplt *self,
                                  struct MidSema_Scope *parent_scope,
                                  const struct MidLexer_Token *toks,
                                  mid_isize start,
                                  struct MidParser_Allocators *allocs,
                                  bool is_param, struct MidDiag_DiagVec *diags);

struct MidParser_TmpltArg
MidParser_copy_tmplt_arg(struct MidParser_TmpltArg *src)
{
    struct MidParser_TmpltArg ret = {.kind = src->kind};

    switch (src->kind) {
    case MIDPARSER_TMPLTARG_NONTYPE:
        ret.non_type = MidParser_copy_expr(&src->non_type);
        break;

    case MIDPARSER_TMPLTARG_TYPE:
        ret.type = MidParser_copy_type(&src->type);
        break;

    case MIDPARSER_TMPLTARG_TMPLT:
        ret.tmplt = src->tmplt;
        break;

    default:
        MID_CRASH("invalid tmplt arg kind");
    }

    return ret;
}

struct MidParser_TmpltArgVec
MidParser_copy_tmplt_argvec(const struct MidParser_TmpltArgVec *src)
{
    struct MidParser_TmpltArgVec ret = {};
    MidGen_dynreserve(&ret, src->len);

    for (mid_isize i = 0; i < src->len; ++i) {
        MidGen_dynpush(&ret, MidParser_copy_tmplt_arg(&src->arr[i]));
    }

    return ret;
}

void MidParser_TmpltInst_deinit(struct MidParser_TmpltInst *self)
{
    MidGen_dyndeinit(&self->args, MidParser_TmpltArg_deinit);
}

void MidParser_Tmplt_deinit(struct MidParser_Tmplt *self)
{
    MidGen_dyndeinit(&self->insts, MidParser_TmpltInst_deinit);
    MidGen_dyndeinit(&self->params);
}

void MidParser_copy_tmplt(struct MidParser_Tmplt *dest,
                          const struct MidParser_Tmplt *src,
                          struct MidSema_Scope *dest_scope,
                          struct MidParser_Allocators *allocs)
{
    *dest = *src;

    MidGen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope = MidSema_create_empty_scope(
        MIDSEMA_SCOPETYPE_TEMPLATE, dest_scope, MIDPARSER_GET_NODE(dest));

    auto params_nodes = MidParser_copy_nodepvec(
        (const struct MidParser_ASTNodePVec *)&src->params,
        MIDPARSER_GET_NODE(dest), dest->scope, allocs);
    dest->params =
        (struct MidParser_TmpltParamPVec){.arr = (void *)params_nodes.arr,
                                          .len = params_nodes.len,
                                          .cap = params_nodes.cap};

    MidGen_bumpmalloc(&allocs->ast, &dest->child);
    MidParser_copy_node(dest->child, src->child, MIDPARSER_GET_NODE(dest),
                        dest->scope, allocs);
}

struct MidSema_Ident *MidParser_tmplt_ident(const struct MidParser_Tmplt *self)
{
    assert(self->scope->idents.len > 0);
    // the last ident is always the templated identifier
    return &self->scope->idents.arr[self->scope->idents.len - 1];
}

void MidParser_TmpltNonTypeParam_deinit(
    struct MidParser_TmpltNonTypeParam *self)
{
    MidParser_Type_deinit(&self->type);
}

void MidParser_TmpltTypeParam_deinit(struct MidParser_TmpltTypeParam *self)
{
    if (self->def_arg) {
        MidParser_Type_deinit(self->def_arg);
        free(self->def_arg);
    }
}

void MidParser_TmpltTmpltParam_deinit(struct MidParser_TmpltTmpltParam *self)
{
    (void)self;
}

void MidParser_TmpltParam_deinit(struct MidParser_TmpltParam *self)
{
    switch (self->kind) {
    case MIDPARSER_TMPLTPARAM_NONTYPE:
        MidParser_TmpltNonTypeParam_deinit(&self->non_type);
        break;

    case MIDPARSER_TMPLTPARAM_TYPE:
        MidParser_TmpltTypeParam_deinit(&self->type);
        break;

    case MIDPARSER_TMPLTPARAM_TMPLT:
        MidParser_TmpltTmpltParam_deinit(&self->tmplt);
        break;

    default:
        MID_CRASH("invalid template param kind");
    }
}

static struct MidSema_Scope *
get_tmplt_scope(const struct MidParser_TmpltParam *param)
{
    return MIDPARSER_GET_PARENT(param)->tmplt.scope;
}

static const char *tmplt_param_name(const struct MidParser_TmpltParam *param)
{
    switch (param->kind) {
    case MIDPARSER_TMPLTPARAM_NONTYPE:
        return param->non_type.name;

    case MIDPARSER_TMPLTPARAM_TYPE:
        return param->type.name;

    case MIDPARSER_TMPLTPARAM_TMPLT:
        return param->tmplt.name;

    default:
        MID_CRASH("invalid tmplt param kind");
    }
}

static const struct MidSema_Ident *
tmplt_param_ident(const struct MidParser_TmpltParam *param)
{
    auto scope = get_tmplt_scope(param);

    mid_isize idx;
    switch (param->kind) {
    case MIDPARSER_TMPLTPARAM_NONTYPE:
        idx = param->non_type.ident_idx;
        break;

    case MIDPARSER_TMPLTPARAM_TYPE:
        idx = param->type.ident_idx;
        break;

    case MIDPARSER_TMPLTPARAM_TMPLT:
        idx = param->tmplt.ident_idx;
        break;

    default:
        MID_CRASH("invalid tmplt param kind");
    }

    return &scope->idents.arr[idx];
}

static void set_ident_idx(struct MidParser_TmpltParam *param,
                          mid_isize ident_idx)
{
    switch (param->kind) {
    case MIDPARSER_TMPLTPARAM_NONTYPE:
        param->non_type.ident_idx = ident_idx;
        break;

    case MIDPARSER_TMPLTPARAM_TYPE:
        param->type.ident_idx = ident_idx;
        break;

    case MIDPARSER_TMPLTPARAM_TMPLT:
        param->tmplt.ident_idx = ident_idx;
        break;

    default:
        MID_CRASH("invalid tmplt param kind");
    }
}

static void
copy_tmplt_nontype_param(struct MidParser_TmpltNonTypeParam *dest,
                         const struct MidParser_TmpltNonTypeParam *src,
                         struct MidParser_Allocators *allocs)
{
    auto ident_idx = dest->ident_idx;
    *dest = *src;
    dest->ident_idx = ident_idx;

    dest->type = MidParser_copy_type(&src->type);

    if (src->def_arg) {
        MidGen_bumpmalloc(&allocs->expr, &dest->def_arg);
        *dest->def_arg = MidParser_copy_expr(src->def_arg);
    }
}

static void copy_tmplt_type_param(struct MidParser_TmpltTypeParam *dest,
                                  const struct MidParser_TmpltTypeParam *src)
{
    if (src->def_arg) {
        dest->def_arg = Mid_malloc(sizeof(*dest->def_arg));
        *dest->def_arg = MidParser_copy_type(src->def_arg);
    }
}

static void copy_tmplt_tmplt_param(struct MidParser_TmpltTmpltParam *dest,
                                   const struct MidParser_TmpltTmpltParam *src,
                                   struct MidParser_Allocators *allocs)
{
    MidGen_bumpmalloc(&allocs->ast, (void **)&dest->tmplt);
    MidParser_copy_node(
        MIDPARSER_GET_NODE(dest->tmplt), MIDPARSER_GET_NODE(src->tmplt),
        (struct MidParser_ASTNode *)dest,
        get_tmplt_scope((const struct MidParser_TmpltParam *)src), allocs);
}

void MidParser_copy_tmplt_param(struct MidParser_TmpltParam *dest,
                                const struct MidParser_TmpltParam *src,
                                struct MidParser_Allocators *allocs)
{
    *dest = *src;

    auto scope = get_tmplt_scope(dest);
    auto old_ident =
        MidSema_add_ident_copy(scope, tmplt_param_ident(src), false, allocs);
    if (old_ident)
        set_ident_idx(dest, old_ident - scope->idents.arr);
    else
        set_ident_idx(dest, scope->idents.len - 1);

    switch (dest->kind) {
    case MIDPARSER_TMPLTPARAM_NONTYPE:
        copy_tmplt_nontype_param(&dest->non_type, &src->non_type, allocs);
        break;

    case MIDPARSER_TMPLTPARAM_TYPE:
        copy_tmplt_type_param(&dest->type, &src->type);
        break;

    case MIDPARSER_TMPLTPARAM_TMPLT:
        copy_tmplt_tmplt_param(&dest->tmplt, &src->tmplt, allocs);
        break;
    }
}

static struct MidSema_Ident *add_ident(const char *name,
                                       struct MidParser_ASTNode *node,
                                       struct MidSema_Scope *scope,
                                       enum MidSema_IdentType type)
{
    auto old = MidSema_add_ident(
        scope, &(struct MidSema_Ident){
                   .name = name, .decl = node, .def = NULL, .type = type});

    if (node->type == MIDPARSER_ASTNODETYPE_TMPLT_PARAM) {
        if (old)
            set_ident_idx(&node->tmplt_param, MidSema_ident_idx(old));
        else
            set_ident_idx(&node->tmplt_param, scope->idents.len - 1);
    }

    return old;
}

static void parse_tmplt_nontype_param(struct MidParser_TmpltNonTypeParam *self,
                                      struct MidSema_Scope *scope,
                                      const struct MidLexer_Token *toks,
                                      mid_isize start, mid_isize *out_end,
                                      struct MidParser_Allocators *allocs,
                                      struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_TmpltNonTypeParam){};

    mid_isize name_idx;
    mid_isize type_end;
    self->type = MidParser_parse_type(toks, start, &type_end, scope, &name_idx,
                                      false, allocs, diags);

    if (name_idx != -1) {
        self->name = toks[name_idx].ident;
        if (add_ident(self->name, MIDPARSER_GET_NODE(self), scope,
                      MIDSEMA_IDENTTYPE_VAR))
            MidGen_dynpush(
                diags, MidDiag_ident_redefined_err(self->name, &toks[name_idx],
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    // default argument
    if (toks[type_end].type == MIDLEXER_TOKENTYPE_ASSIGN) {
        mid_isize expr_start = type_end + 1;
        MidGen_bumpmalloc(&allocs->expr, &self->def_arg);
        *self->def_arg = MidParser_parse_expr(toks, expr_start,
                                              MIDPARSER_TMPLT_PARAM_ENDTYPES,
                                              out_end, scope, diags);
    } else if (out_end) {
        *out_end = type_end;
    }
}

static void parse_tmplt_type_param(struct MidParser_TmpltTypeParam *self,
                                   struct MidSema_Scope *scope,
                                   const struct MidLexer_Token *toks,
                                   mid_isize start, mid_isize *out_end,
                                   struct MidParser_Allocators *allocs,
                                   struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_TmpltTypeParam){};

    mid_isize name_idx = start + 1;
    mid_isize assign_idx = name_idx;
    if (toks[name_idx].type == MIDLEXER_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        self->name = toks[name_idx].ident;
        if (add_ident(self->name, MIDPARSER_GET_NODE(self), scope,
                      MIDSEMA_IDENTTYPE_TYPEDEF))
            MidGen_dynpush(
                diags, MidDiag_ident_redefined_err(self->name, &toks[name_idx],
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    // default argument
    if (toks[assign_idx].type == MIDLEXER_TOKENTYPE_ASSIGN) {
        mid_isize type_start = assign_idx + 1;
        mid_isize def_name_idx;
        self->def_arg = Mid_malloc(sizeof(*self->def_arg));
        *self->def_arg =
            MidParser_parse_type(toks, type_start, out_end, scope,
                                 &def_name_idx, true, allocs, diags);
    } else if (out_end) {
        *out_end = assign_idx;
    }
}

static void parse_tmplt_tmplt_param(struct MidParser_TmpltTmpltParam *self,
                                    struct MidSema_Scope *scope,
                                    const struct MidLexer_Token *toks,
                                    mid_isize start, mid_isize *out_end,
                                    struct MidParser_Allocators *allocs,
                                    struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_TmpltTmpltParam){};

    MidGen_bumpmalloc(&allocs->ast, (void **)&self->tmplt);
    MIDPARSER_GET_PARENT(self->tmplt) = MIDPARSER_GET_NODE(self);
    MIDPARSER_GET_START(self->tmplt) = &toks[start];
    MIDPARSER_GET_TYPE(self->tmplt) = MIDPARSER_ASTNODETYPE_TMPLT;

    mid_isize name_idx =
        parse_tmplt_impl(self->tmplt, scope, toks, start, allocs, true, diags);

    mid_isize assign_idx = name_idx;
    if (toks[name_idx].type == MIDLEXER_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        self->name = toks[name_idx].ident;
        if (add_ident(self->name, MIDPARSER_GET_NODE(self), scope,
                      MIDSEMA_IDENTTYPE_TMPLT_CLASS))
            MidGen_dynpush(
                diags, MidDiag_ident_redefined_err(self->name, &toks[name_idx],
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    // no default argument
    if (toks[assign_idx].type != MIDLEXER_TOKENTYPE_ASSIGN) {
        if (out_end)
            *out_end = assign_idx;
        return;
    }

    MID_CRASH(
        "default argument for template template parameter not yet supported");
}

void parse_tmplt_param(struct MidParser_TmpltParam *self,
                       struct MidSema_Scope *scope,
                       const struct MidLexer_Token *toks, mid_isize start,
                       mid_isize *out_end, struct MidParser_Allocators *allocs,
                       struct MidDiag_DiagVec *diags)
{
    MIDPARSER_GET_START(self) = &toks[start];
    MIDPARSER_GET_TYPE(self) = MIDPARSER_ASTNODETYPE_TMPLT_PARAM;

    if (toks[start].type == MIDLEXER_TOKENTYPE_TEMPLATE) {
        self->kind = MIDPARSER_TMPLTPARAM_TMPLT;
        parse_tmplt_tmplt_param(&self->tmplt, scope, toks, start, out_end,
                                allocs, diags);
    } else if (toks[start].type == MIDLEXER_TOKENTYPE_CLASS ||
               toks[start].type == MIDLEXER_TOKENTYPE_TYPENAME) {
        self->kind = MIDPARSER_TMPLTPARAM_TYPE;
        parse_tmplt_type_param(&self->type, scope, toks, start, out_end, allocs,
                               diags);
    } else {
        self->kind = MIDPARSER_TMPLTPARAM_NONTYPE;
        parse_tmplt_nontype_param(&self->non_type, scope, toks, start, out_end,
                                  allocs, diags);
    }
}

static struct MidParser_TmpltParamPVec parse_tmplt_param_list(
    struct MidParser_ASTNode *parent, struct MidSema_Scope *scope,
    const struct MidLexer_Token *toks, mid_isize l_angle,
    mid_isize *out_r_angle, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags)
{
    struct MidParser_TmpltParamPVec params = {};

    mid_isize r_angle = MidParser_find_twin_angle(toks, l_angle, MID_ISIZE_MAX);
    if (out_r_angle)
        *out_r_angle = r_angle == -1 ? l_angle : r_angle;

    if (r_angle == -1) {
        MidGen_dynpush(diags,
                       MidDiag_expected_token_err(">", &toks[l_angle],
                                                  MIDDIAG_ERR_MISSING_ANGLE));
        return params;
    }

    for (mid_isize i = l_angle + 1; i < r_angle; ++i) {
        struct MidParser_TmpltParam *param;
        MidGen_bumpmalloc(&allocs->ast, (void **)&param);

        MIDPARSER_GET_PARENT(param) = parent;
        parse_tmplt_param(param, scope, toks, i, &i, allocs, diags);

        MidGen_dynpush(&params, param);
    }

    return params;
}

static struct MidSema_Scope *create_scope(struct MidSema_Scope *parent,
                                          struct MidParser_Tmplt *self,
                                          struct MidParser_Allocators *allocs)
{
    struct MidSema_Scope *ret;
    MidGen_bumpmalloc(&allocs->scope, &ret);
    *ret = (struct MidSema_Scope){.parent = parent,
                                  .node = MIDPARSER_GET_NODE(self),
                                  .type = MIDSEMA_SCOPETYPE_TEMPLATE};
    MidGen_dynpush(&parent->childs, ret);

    return ret;
}

// is_param     - is the template a template template parameter? if so the
//                function stops after the type parameter key
static mid_isize parse_tmplt_impl(struct MidParser_Tmplt *self,
                                  struct MidSema_Scope *parent_scope,
                                  const struct MidLexer_Token *toks,
                                  mid_isize start,
                                  struct MidParser_Allocators *allocs,
                                  bool is_param, struct MidDiag_DiagVec *diags)
{
    assert(toks[start].type == MIDLEXER_TOKENTYPE_TEMPLATE);

    *self = (struct MidParser_Tmplt){};

    mid_isize l_angle = start + 1;
    if (toks[l_angle].type != MIDLEXER_TTALIAS_L_ANGLE) {
        MidGen_dynpush(diags,
                       MidDiag_expected_token_err("<", &toks[start],
                                                  MIDDIAG_ERR_MISSING_ANGLE));
        return start;
    }

    self->scope = create_scope(parent_scope, self, allocs);

    mid_isize r_angle;
    self->params =
        parse_tmplt_param_list(MIDPARSER_GET_NODE(self), self->scope, toks,
                               l_angle, &r_angle, allocs, diags);

    if (is_param) {
        if (toks[r_angle + 1].type != MIDLEXER_TOKENTYPE_CLASS) {
            MidGen_dynpush(
                diags, MidDiag_expected_token_err("class", &toks[r_angle + 1],
                                                  MIDDIAG_ERR_MISSING_TOKEN));
            return r_angle + 1;
        } else {
            return r_angle + 2;
        }
    } else {
        return r_angle + 1;
    }
}

static void check_child_valid(const struct MidParser_Tmplt *tmplt,
                              struct MidDiag_DiagVec *diags)
{
    if (tmplt->child->type != MIDPARSER_ASTNODETYPE_FUNC_DECL &&
        tmplt->child->type != MIDPARSER_ASTNODETYPE_CLASS) {
        MidGen_dynpush(
            diags,
            ((struct MidDiag_Diag){
                .pos = tmplt->child->start->pos,
                .line = tmplt->child->start->line,
                .msg = MidPrint_fmt_to_str("statement can not be a template"),
                .err = MIDDIAG_ERR_BAD_TEMPLATE,
                .type = MIDDIAG_TYPE_ERROR}));
    }
}

mid_isize MidParser_parse_tmplt(struct MidParser_Tmplt *self,
                                struct MidSema_Scope *parent_scope,
                                const struct MidLexer_Token *toks,
                                mid_isize start,
                                struct MidParser_Allocators *allocs,
                                struct MidDiag_DiagVec *diags)
{
    mid_isize child_start =
        parse_tmplt_impl(self, parent_scope, toks, start, allocs, false, diags);

    mid_isize child_end;
    self->child = MidParser_parse_node(
        toks, child_start, &child_end, MIDPARSER_GET_NODE(self), self->scope,
        (struct MidParser_ParseNodeFlags){}, allocs, diags);

    check_child_valid(self, diags);

    return child_end;
}

void MidParser_TmpltArg_deinit(struct MidParser_TmpltArg *self)
{
    switch (self->kind) {
    case MIDPARSER_TMPLTARG_NONTYPE:
        MidParser_Expr_deinit(&self->non_type);
        break;

    case MIDPARSER_TMPLTARG_TYPE:
        MidParser_Type_deinit(&self->type);
        break;

    default:
        break;
    }
}

static bool is_tmplt_tmplt_arg(const struct MidLexer_Token *tok,
                               struct MidSema_Scope *scope,
                               struct MidSema_Ident **out_ident)
{
    if (tok->type != MIDLEXER_TOKENTYPE_IDENTIFIER)
        return false;

    auto ident = MidSema_find_ident(scope, tok->ident);
    if (out_ident)
        *out_ident = ident;
    if (!ident)
        return false;

    return MidSema_ident_is_tmplt(ident->type);
}

static struct MidParser_TmpltArg
parse_tmplt_arg(const struct MidLexer_Token *toks, mid_isize start,
                mid_isize *out_end, struct MidSema_Scope *scope,
                struct MidParser_Allocators *allocs,
                struct MidDiag_DiagVec *diags)
{
    struct MidParser_TmpltArg arg = {};

    struct MidSema_Ident *ident;
    if (is_tmplt_tmplt_arg(&toks[start], scope, &ident)) {
        arg.kind = MIDPARSER_TMPLTARG_TMPLT;
        arg.tmplt = MidSema_create_identptr(ident);
    } else if (MidParser_valid_type_start(toks, start, scope)) {
        arg.kind = MIDPARSER_TMPLTARG_TYPE;
        arg.type = MidParser_parse_type(toks, start, out_end, scope, NULL, true,
                                        allocs, diags);
    } else {
        arg.kind = MIDPARSER_TMPLTARG_NONTYPE;
        arg.non_type = MidParser_parse_expr(
            toks, start, MIDPARSER_TMPLT_ARG_ENDTYPES, out_end, scope, diags);
    }

    return arg;
}

struct MidParser_TmpltArgVec
MidParser_parse_tmplt_args(const struct MidLexer_Token *toks, mid_isize l_angle,
                           mid_isize *out_r_angle, struct MidSema_Scope *scope,
                           struct MidParser_Allocators *allocs,
                           struct MidDiag_DiagVec *diags)
{
    struct MidParser_TmpltArgVec args = {};

    if (toks[l_angle].type != MIDLEXER_TTALIAS_L_ANGLE) {
        MidGen_dynpush(diags,
                       MidDiag_expected_token_err("'<'", &toks[l_angle],
                                                  MIDDIAG_ERR_MISSING_ANGLE));
        if (out_r_angle)
            *out_r_angle = l_angle;
        return args;
    }

    mid_isize r_angle = MidParser_find_twin_angle(toks, l_angle, MID_ISIZE_MAX);
    if (r_angle == -1) {
        MidGen_dynpush(diags,
                       MidDiag_expected_token_err("'<'", &toks[l_angle],
                                                  MIDDIAG_ERR_MISSING_ANGLE));
        if (out_r_angle)
            *out_r_angle = l_angle;
        return args;
    }

    if (out_r_angle)
        *out_r_angle = r_angle;

    for (mid_isize i = l_angle + 1; i < r_angle; ++i) {
        auto arg = parse_tmplt_arg(toks, i, &i, scope, allocs, diags);
        if (i < r_angle && toks[i].type != MIDLEXER_TOKENTYPE_COMMA)
            MidGen_dynpush(
                diags, MidDiag_expected_token_err("','", &toks[i],
                                                  MIDDIAG_ERR_MISSING_COMMA));

        MidGen_dynpush(&args, arg);
    }

    return args;
}

mid_isize MidParser_tmplt_param_idx(const struct MidParser_Tmplt *tmplt,
                                    const char *name)
{
    for (mid_isize i = 0; i < tmplt->params.len; ++i) {
        const char *p_name = tmplt_param_name(tmplt->params.arr[i]);
        if (!strcmp(name, p_name))
            return i;
    }

    return -1;
}
