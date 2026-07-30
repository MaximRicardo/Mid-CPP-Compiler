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

static isize_t parse_tmplt_impl(struct Parser_Tmplt *self,
                                struct Sema_Scope *parent_scope,
                                const struct Lexer_Token *toks, isize_t start,
                                struct Parser_Allocators *allocs, bool is_param,
                                struct DiagVec *diags);

struct Parser_TmpltArg Parser_copy_tmplt_arg(struct Parser_TmpltArg *src)
{
    struct Parser_TmpltArg ret = {.kind = src->kind};

    switch (src->kind) {
    case PARSER_TMPLTARG_NONTYPE:
        ret.non_type = Parser_copy_expr(&src->non_type);
        break;

    case PARSER_TMPLTARG_TYPE:
        ret.type = Parser_copy_type(&src->type);
        break;

    case PARSER_TMPLTARG_TMPLT:
        ret.tmplt = src->tmplt;
        break;

    default:
        CRASH("invalid tmplt arg kind");
    }

    return ret;
}

struct Parser_TmpltArgVec
Parser_copy_tmplt_argvec(const struct Parser_TmpltArgVec *src)
{
    struct Parser_TmpltArgVec ret = {};
    gen_dynreserve(&ret, src->len);

    for (isize_t i = 0; i < src->len; ++i) {
        gen_dynpush(&ret, Parser_copy_tmplt_arg(&src->arr[i]));
    }

    return ret;
}

void Parser_TmpltInst_deinit(struct Parser_TmpltInst *self)
{
    gen_dyndeinit(&self->args, Parser_TmpltArg_deinit);
}

void Parser_Tmplt_deinit(struct Parser_Tmplt *self)
{
    gen_dyndeinit(&self->insts, Parser_TmpltInst_deinit);
    gen_dyndeinit(&self->params);
}

void Parser_copy_tmplt(struct Parser_Tmplt *dest,
                       const struct Parser_Tmplt *src,
                       struct Sema_Scope *dest_scope,
                       struct Parser_Allocators *allocs)
{
    *dest = *src;

    gen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope = Sema_create_empty_scope(SEMA_SCOPETYPE_TEMPLATE, dest_scope,
                                           PARSER_GET_NODE(dest));

    auto params_nodes =
        Parser_copy_nodepvec((const struct Parser_ASTNodePVec *)&src->params,
                             PARSER_GET_NODE(dest), dest->scope, allocs);
    dest->params =
        (struct Parser_TmpltParamPVec){.arr = (void *)params_nodes.arr,
                                       .len = params_nodes.len,
                                       .cap = params_nodes.cap};

    gen_bumpmalloc(&allocs->ast, &dest->child);
    Parser_copy_node(dest->child, src->child, PARSER_GET_NODE(dest),
                     dest->scope, allocs);
}

struct Sema_Ident *Parser_tmplt_ident(const struct Parser_Tmplt *self)
{
    assert(self->scope->idents.len > 0);
    // the last ident is always the templated identifier
    return &self->scope->idents.arr[self->scope->idents.len - 1];
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

static struct Sema_Scope *get_tmplt_scope(const struct Parser_TmpltParam *param)
{
    return PARSER_GET_PARENT(param)->tmplt.scope;
}

static const char *tmplt_param_name(const struct Parser_TmpltParam *param)
{
    switch (param->kind) {
    case PARSER_TMPLTPARAM_NONTYPE:
        return param->non_type.name;

    case PARSER_TMPLTPARAM_TYPE:
        return param->type.name;

    case PARSER_TMPLTPARAM_TMPLT:
        return param->tmplt.name;

    default:
        CRASH("invalid tmplt param kind");
    }
}

static const struct Sema_Ident *
tmplt_param_ident(const struct Parser_TmpltParam *param)
{
    auto scope = get_tmplt_scope(param);

    isize_t idx;
    switch (param->kind) {
    case PARSER_TMPLTPARAM_NONTYPE:
        idx = param->non_type.ident_idx;
        break;

    case PARSER_TMPLTPARAM_TYPE:
        idx = param->type.ident_idx;
        break;

    case PARSER_TMPLTPARAM_TMPLT:
        idx = param->tmplt.ident_idx;
        break;

    default:
        CRASH("invalid tmplt param kind");
    }

    return &scope->idents.arr[idx];
}

static void set_ident_idx(struct Parser_TmpltParam *param, isize_t ident_idx)
{
    switch (param->kind) {
    case PARSER_TMPLTPARAM_NONTYPE:
        param->non_type.ident_idx = ident_idx;
        break;

    case PARSER_TMPLTPARAM_TYPE:
        param->type.ident_idx = ident_idx;
        break;

    case PARSER_TMPLTPARAM_TMPLT:
        param->tmplt.ident_idx = ident_idx;
        break;

    default:
        CRASH("invalid tmplt param kind");
    }
}

static void copy_tmplt_nontype_param(struct Parser_TmpltNonTypeParam *dest,
                                     const struct Parser_TmpltNonTypeParam *src,
                                     struct Parser_Allocators *allocs)
{
    auto ident_idx = dest->ident_idx;
    *dest = *src;
    dest->ident_idx = ident_idx;

    dest->type = Parser_copy_type(&src->type);

    if (src->def_arg) {
        gen_bumpmalloc(&allocs->expr, &dest->def_arg);
        *dest->def_arg = Parser_copy_expr(src->def_arg);
    }
}

static void copy_tmplt_type_param(struct Parser_TmpltTypeParam *dest,
                                  const struct Parser_TmpltTypeParam *src)
{
    if (src->def_arg) {
        dest->def_arg = mid_malloc(sizeof(*dest->def_arg));
        *dest->def_arg = Parser_copy_type(src->def_arg);
    }
}

static void copy_tmplt_tmplt_param(struct Parser_TmpltTmpltParam *dest,
                                   const struct Parser_TmpltTmpltParam *src,
                                   struct Parser_Allocators *allocs)
{
    gen_bumpmalloc(&allocs->ast, (void **)&dest->tmplt);
    Parser_copy_node(PARSER_GET_NODE(dest->tmplt), PARSER_GET_NODE(src->tmplt),
                     (struct Parser_ASTNode *)dest,
                     get_tmplt_scope((const struct Parser_TmpltParam *)src),
                     allocs);
}

void Parser_copy_tmplt_param(struct Parser_TmpltParam *dest,
                             const struct Parser_TmpltParam *src,
                             struct Parser_Allocators *allocs)
{
    *dest = *src;

    auto scope = get_tmplt_scope(dest);
    auto old_ident =
        Sema_add_ident_copy(scope, tmplt_param_ident(src), false, allocs);
    if (old_ident)
        set_ident_idx(dest, old_ident - scope->idents.arr);
    else
        set_ident_idx(dest, scope->idents.len - 1);

    switch (dest->kind) {
    case PARSER_TMPLTPARAM_NONTYPE:
        copy_tmplt_nontype_param(&dest->non_type, &src->non_type, allocs);
        break;

    case PARSER_TMPLTPARAM_TYPE:
        copy_tmplt_type_param(&dest->type, &src->type);
        break;

    case PARSER_TMPLTPARAM_TMPLT:
        copy_tmplt_tmplt_param(&dest->tmplt, &src->tmplt, allocs);
        break;
    }
}

static struct Sema_Ident *add_ident(const char *name,
                                    struct Parser_ASTNode *node,
                                    struct Sema_Scope *scope,
                                    enum Sema_IdentType type)
{
    auto old = Sema_add_ident(
        scope, &(struct Sema_Ident){
                   .name = name, .decl = node, .def = NULL, .type = type});

    if (node->type == PARSER_ASTNODETYPE_TMPLT_PARAM) {
        if (old)
            set_ident_idx(&node->tmplt_param, Sema_ident_idx(old));
        else
            set_ident_idx(&node->tmplt_param, scope->idents.len - 1);
    }

    return old;
}

static void parse_tmplt_nontype_param(struct Parser_TmpltNonTypeParam *self,
                                      struct Sema_Scope *scope,
                                      const struct Lexer_Token *toks,
                                      isize_t start, isize_t *out_end,
                                      struct Parser_Allocators *allocs,
                                      struct DiagVec *diags)
{
    *self = (struct Parser_TmpltNonTypeParam){};

    isize_t name_idx;
    isize_t type_end;
    self->type = Parser_parse_type(toks, start, &type_end, scope, &name_idx,
                                   false, allocs, diags);

    if (name_idx != -1) {
        self->name = toks[name_idx].ident;
        if (add_ident(self->name, PARSER_GET_NODE(self), scope,
                      SEMA_IDENTTYPE_VAR))
            gen_dynpush(diags,
                        Diag_ident_redefined_err(self->name, &toks[name_idx],
                                                 ERRORTYPE_BAD_IDENTIFIER));
    }

    // default argument
    if (toks[type_end].type == LEXER_TOKENTYPE_ASSIGN) {
        isize_t expr_start = type_end + 1;
        gen_bumpmalloc(&allocs->expr, &self->def_arg);
        *self->def_arg =
            Parser_parse_expr(toks, expr_start, PARSER_TMPLT_PARAM_ENDTYPES,
                              out_end, scope, diags);
    } else if (out_end) {
        *out_end = type_end;
    }
}

static void parse_tmplt_type_param(struct Parser_TmpltTypeParam *self,
                                   struct Sema_Scope *scope,
                                   const struct Lexer_Token *toks,
                                   isize_t start, isize_t *out_end,
                                   struct Parser_Allocators *allocs,
                                   struct DiagVec *diags)
{
    *self = (struct Parser_TmpltTypeParam){};

    isize_t name_idx = start + 1;
    isize_t assign_idx = name_idx;
    if (toks[name_idx].type == LEXER_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        self->name = toks[name_idx].ident;
        if (add_ident(self->name, PARSER_GET_NODE(self), scope,
                      SEMA_IDENTTYPE_TYPEDEF))
            gen_dynpush(diags,
                        Diag_ident_redefined_err(self->name, &toks[name_idx],
                                                 ERRORTYPE_BAD_IDENTIFIER));
    }

    // default argument
    if (toks[assign_idx].type == LEXER_TOKENTYPE_ASSIGN) {
        isize_t type_start = assign_idx + 1;
        isize_t def_name_idx;
        self->def_arg = mid_malloc(sizeof(*self->def_arg));
        *self->def_arg = Parser_parse_type(toks, type_start, out_end, scope,
                                           &def_name_idx, true, allocs, diags);
    } else if (out_end) {
        *out_end = assign_idx;
    }
}

static void parse_tmplt_tmplt_param(struct Parser_TmpltTmpltParam *self,
                                    struct Sema_Scope *scope,
                                    const struct Lexer_Token *toks,
                                    isize_t start, isize_t *out_end,
                                    struct Parser_Allocators *allocs,
                                    struct DiagVec *diags)
{
    *self = (struct Parser_TmpltTmpltParam){};

    gen_bumpmalloc(&allocs->ast, (void **)&self->tmplt);
    PARSER_GET_PARENT(self->tmplt) = PARSER_GET_NODE(self);
    PARSER_GET_START(self->tmplt) = &toks[start];
    PARSER_GET_TYPE(self->tmplt) = PARSER_ASTNODETYPE_TMPLT;

    isize_t name_idx =
        parse_tmplt_impl(self->tmplt, scope, toks, start, allocs, true, diags);

    isize_t assign_idx = name_idx;
    if (toks[name_idx].type == LEXER_TOKENTYPE_IDENTIFIER) {
        ++assign_idx;
        self->name = toks[name_idx].ident;
        if (add_ident(self->name, PARSER_GET_NODE(self), scope,
                      SEMA_IDENTTYPE_TMPLT_CLASS))
            gen_dynpush(diags,
                        Diag_ident_redefined_err(self->name, &toks[name_idx],
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

void parse_tmplt_param(struct Parser_TmpltParam *self, struct Sema_Scope *scope,
                       const struct Lexer_Token *toks, isize_t start,
                       isize_t *out_end, struct Parser_Allocators *allocs,
                       struct DiagVec *diags)
{
    PARSER_GET_START(self) = &toks[start];
    PARSER_GET_TYPE(self) = PARSER_ASTNODETYPE_TMPLT_PARAM;

    if (toks[start].type == LEXER_TOKENTYPE_TEMPLATE) {
        self->kind = PARSER_TMPLTPARAM_TMPLT;
        parse_tmplt_tmplt_param(&self->tmplt, scope, toks, start, out_end,
                                allocs, diags);
    } else if (toks[start].type == LEXER_TOKENTYPE_CLASS ||
               toks[start].type == LEXER_TOKENTYPE_TYPENAME) {
        self->kind = PARSER_TMPLTPARAM_TYPE;
        parse_tmplt_type_param(&self->type, scope, toks, start, out_end, allocs,
                               diags);
    } else {
        self->kind = PARSER_TMPLTPARAM_NONTYPE;
        parse_tmplt_nontype_param(&self->non_type, scope, toks, start, out_end,
                                  allocs, diags);
    }
}

static struct Parser_TmpltParamPVec
parse_tmplt_param_list(struct Parser_ASTNode *parent, struct Sema_Scope *scope,
                       const struct Lexer_Token *toks, isize_t l_angle,
                       isize_t *out_r_angle, struct Parser_Allocators *allocs,
                       struct DiagVec *diags)
{
    struct Parser_TmpltParamPVec params = {};

    isize_t r_angle = Parser_find_twin_angle(toks, l_angle, ISIZE_MAX);
    if (out_r_angle)
        *out_r_angle = r_angle == -1 ? l_angle : r_angle;

    if (r_angle == -1) {
        gen_dynpush(diags, Diag_expected_token_err(">", &toks[l_angle],
                                                   ERRORTYPE_MISSING_ANGLE));
        return params;
    }

    for (isize_t i = l_angle + 1; i < r_angle; ++i) {
        struct Parser_TmpltParam *param;
        gen_bumpmalloc(&allocs->ast, (void **)&param);

        PARSER_GET_PARENT(param) = parent;
        parse_tmplt_param(param, scope, toks, i, &i, allocs, diags);

        gen_dynpush(&params, param);
    }

    return params;
}

static struct Sema_Scope *create_scope(struct Sema_Scope *parent,
                                       struct Parser_Tmplt *self,
                                       struct Parser_Allocators *allocs)
{
    struct Sema_Scope *ret;
    gen_bumpmalloc(&allocs->scope, &ret);
    *ret = (struct Sema_Scope){.parent = parent,
                               .node = PARSER_GET_NODE(self),
                               .type = SEMA_SCOPETYPE_TEMPLATE};
    gen_dynpush(&parent->childs, ret);

    return ret;
}

// is_param     - is the template a template template parameter? if so the
//                function stops after the type parameter key
static isize_t parse_tmplt_impl(struct Parser_Tmplt *self,
                                struct Sema_Scope *parent_scope,
                                const struct Lexer_Token *toks, isize_t start,
                                struct Parser_Allocators *allocs, bool is_param,
                                struct DiagVec *diags)
{
    assert(toks[start].type == LEXER_TOKENTYPE_TEMPLATE);

    *self = (struct Parser_Tmplt){};

    isize_t l_angle = start + 1;
    if (toks[l_angle].type != LEXER_TTALIAS_L_ANGLE) {
        gen_dynpush(diags, Diag_expected_token_err("<", &toks[start],
                                                   ERRORTYPE_MISSING_ANGLE));
        return start;
    }

    self->scope = create_scope(parent_scope, self, allocs);

    isize_t r_angle;
    self->params =
        parse_tmplt_param_list(PARSER_GET_NODE(self), self->scope, toks,
                               l_angle, &r_angle, allocs, diags);

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

static void check_child_valid(const struct Parser_Tmplt *tmplt,
                              struct DiagVec *diags)
{
    if (tmplt->child->type != PARSER_ASTNODETYPE_FUNC_DECL &&
        tmplt->child->type != PARSER_ASTNODETYPE_CLASS) {
        gen_dynpush(diags,
                    ((struct Diag){.pos = tmplt->child->start->pos,
                                   .line = tmplt->child->start->line,
                                   .msg = Print_fmt_to_str(
                                       "statement can not be a template"),
                                   .err = ERRORTYPE_BAD_TEMPLATE,
                                   .type = DIAGTYPE_ERROR}));
    }
}

isize_t Parser_parse_tmplt(struct Parser_Tmplt *self,
                           struct Sema_Scope *parent_scope,
                           const struct Lexer_Token *toks, isize_t start,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    isize_t child_start =
        parse_tmplt_impl(self, parent_scope, toks, start, allocs, false, diags);

    isize_t child_end;
    self->child = Parser_parse_node(
        toks, child_start, &child_end, PARSER_GET_NODE(self), self->scope,
        (struct Parser_ParseNodeFlags){}, allocs, diags);

    check_child_valid(self, diags);

    return child_end;
}

void Parser_TmpltArg_deinit(struct Parser_TmpltArg *self)
{
    switch (self->kind) {
    case PARSER_TMPLTARG_NONTYPE:
        Parser_Expr_deinit(&self->non_type);
        break;

    case PARSER_TMPLTARG_TYPE:
        Parser_Type_deinit(&self->type);
        break;

    default:
        break;
    }
}

static bool is_tmplt_tmplt_arg(const struct Lexer_Token *tok,
                               struct Sema_Scope *scope,
                               struct Sema_Ident **out_ident)
{
    if (tok->type != LEXER_TOKENTYPE_IDENTIFIER)
        return false;

    auto ident = Sema_find_ident(scope, tok->ident);
    if (out_ident)
        *out_ident = ident;
    if (!ident)
        return false;

    return Sema_ident_is_tmplt(ident->type);
}

static struct Parser_TmpltArg parse_tmplt_arg(const struct Lexer_Token *toks,
                                              isize_t start, isize_t *out_end,
                                              struct Sema_Scope *scope,
                                              struct Parser_Allocators *allocs,
                                              struct DiagVec *diags)
{
    struct Parser_TmpltArg arg = {};

    struct Sema_Ident *ident;
    if (is_tmplt_tmplt_arg(&toks[start], scope, &ident)) {
        arg.kind = PARSER_TMPLTARG_TMPLT;
        arg.tmplt = Sema_create_identptr(ident);
    } else if (Parser_valid_type_start(toks, start, scope)) {
        arg.kind = PARSER_TMPLTARG_TYPE;
        arg.type = Parser_parse_type(toks, start, out_end, scope, NULL, true,
                                     allocs, diags);
    } else {
        arg.kind = PARSER_TMPLTARG_NONTYPE;
        arg.non_type = Parser_parse_expr(toks, start, PARSER_TMPLT_ARG_ENDTYPES,
                                         out_end, scope, diags);
    }

    return arg;
}

struct Parser_TmpltArgVec
Parser_parse_tmplt_args(const struct Lexer_Token *toks, isize_t l_angle,
                        isize_t *out_r_angle, struct Sema_Scope *scope,
                        struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    struct Parser_TmpltArgVec args = {};

    if (toks[l_angle].type != LEXER_TTALIAS_L_ANGLE) {
        gen_dynpush(diags, Diag_expected_token_err("'<'", &toks[l_angle],
                                                   ERRORTYPE_MISSING_ANGLE));
        if (out_r_angle)
            *out_r_angle = l_angle;
        return args;
    }

    isize_t r_angle = Parser_find_twin_angle(toks, l_angle, ISIZE_MAX);
    if (r_angle == -1) {
        gen_dynpush(diags, Diag_expected_token_err("'<'", &toks[l_angle],
                                                   ERRORTYPE_MISSING_ANGLE));
        if (out_r_angle)
            *out_r_angle = l_angle;
        return args;
    }

    if (out_r_angle)
        *out_r_angle = r_angle;

    for (isize_t i = l_angle + 1; i < r_angle; ++i) {
        auto arg = parse_tmplt_arg(toks, i, &i, scope, allocs, diags);
        if (i < r_angle && toks[i].type != LEXER_TOKENTYPE_COMMA)
            gen_dynpush(diags, Diag_expected_token_err(
                                   "','", &toks[i], ERRORTYPE_MISSING_COMMA));

        gen_dynpush(&args, arg);
    }

    return args;
}

isize_t Parser_tmplt_param_idx(const struct Parser_Tmplt *tmplt,
                               const char *name)
{
    for (isize_t i = 0; i < tmplt->params.len; ++i) {
        const char *p_name = tmplt_param_name(tmplt->params.arr[i]);
        if (!strcmp(name, p_name))
            return i;
    }

    return -1;
}
