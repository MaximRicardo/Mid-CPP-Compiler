#include "func_decl.h"
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
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <string.h>

static struct MidDiag_Diag
missing_default_arg_err(const char *func, const struct MidLexer_Token *tok,
                        enum MidDiag_ErrT err_type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg =
            MidPrint_fmt_to_str("function '%s' missing default argument", func),
        .err = err_type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

void MidParser_FuncDecl_deinit(struct MidParser_FuncDecl *self)
{
    MidGen_dyndeinit(&self->nodes);
    MidGen_dyndeinit(&self->params);
    MidParser_Type_deinit(&self->ret);
}

void MidParser_copy_func_decl(struct MidParser_FuncDecl *dest,
                              const struct MidParser_FuncDecl *src,
                              struct MidSema_Scope *dest_scope,
                              struct MidParser_Allocators *allocs)
{
    *dest = *src;

    auto old_ident = MidSema_add_ident_copy(
        dest_scope, MidParser_func_ident(src), false, allocs);
    if (old_ident)
        dest->ident_idx = old_ident - dest_scope->idents.arr;
    else
        dest->ident_idx = dest_scope->idents.len - 1;

    dest->ret = MidParser_copy_type(&src->ret);

    MidGen_bumpmalloc(&allocs->scope, &dest->param_scope);
    *dest->param_scope = MidSema_create_empty_scope(
        src->param_scope->type, dest_scope, MIDPARSER_GET_NODE(dest));

    auto params_nodes = MidParser_copy_nodepvec(
        (struct MidParser_ASTNodePVec *)&src->params, MIDPARSER_GET_NODE(dest),
        dest->param_scope, allocs);
    dest->params =
        (struct MidParser_VarDeclPVec){.arr = (void *)params_nodes.arr,
                                       .len = params_nodes.len,
                                       .cap = params_nodes.cap};

    if (src->nodes.len > 0) {
        struct MidSema_Scope *def_scope;
        MidGen_bumpmalloc(&allocs->scope, &def_scope);
        *def_scope = (struct MidSema_Scope){.parent = dest_scope,
                                            .node = MIDPARSER_GET_NODE(dest),
                                            .type = MIDSEMA_SCOPETYPE_FUNC};
        MidParser_func_ident(dest)->func_info.def_scope = def_scope;
        dest->nodes = MidParser_copy_nodepvec(
            &src->nodes, MIDPARSER_GET_NODE(dest), def_scope, allocs);
    }
}

struct MidSema_Scope *
MidParser_func_parent(const struct MidParser_FuncDecl *func)
{
    auto ret = func->param_scope->parent;
    assert(ret);
    return ret;
}

struct MidSema_Ident *
MidParser_func_ident(const struct MidParser_FuncDecl *func)
{
    assert(func->ident_idx != -1);
    auto parent = MidParser_func_parent(func);
    return &parent->idents.arr[func->ident_idx];
}

// accounts for functions taking a singular void parameter
// example: int f(void)
static void account_for_void_param(struct MidParser_VarDeclPVec *params)
{
    if (params->len == 1 &&
        MidParser_type_is_void(&params->arr[0]->insts.arr[0]->type)) {
        MidGen_dyndeinit(params);
    }
}

struct MidParser_VarDeclPVec MidParser_parse_func_params(
    const struct MidLexer_Token *toks, mid_isize lparen, mid_isize *out_rparen,
    struct MidParser_ASTNode *parent, struct MidSema_Scope *scope,
    bool add_to_scope, bool *out_variadic, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags)
{
    struct MidParser_VarDeclPVec params = {};

    mid_isize rparen = MidParser_find_twin_paren(toks, lparen, MID_ISIZE_MAX);
    if (out_rparen)
        *out_rparen = rparen;

    if (rparen == -1) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("')'", &toks[lparen],
                                               MIDDIAG_ERR_MISSING_PAREN));
        return params;
    }

    if (out_variadic)
        *out_variadic = false;

    for (mid_isize i = lparen + 1; i < rparen; ++i) {
        if (toks[i].type == MIDLEXER_TOKENTYPE_ELLIPSIS) {
            if (out_variadic)
                *out_variadic = true;
            if (i + 1 < rparen)
                MidGen_dynpush(diags, MidDiag_expected_token_err(
                                       "')'", &toks[lparen],
                                       MIDDIAG_ERR_MISSING_PAREN));
            break;
        } else {
            struct MidParser_ASTNode *child;
            MidGen_bumpmalloc(&allocs->ast, &child);
            *child = (struct MidParser_ASTNode){
                .parent = parent,
                .start = &toks[i],
                .type = MIDPARSER_ASTNODETYPE_VAR_DECL};
            i = MidParser_parse_var_decl(
                &child->var_decl, toks, i, MIDPARSER_PARAM_ENDTYPES,
                (struct MidParser_ParseVarDeclFlags){
                    .add_to_scope = add_to_scope, .single_inst = true},
                scope, allocs, diags);
            MidGen_dynpush(&params, (void *)child);
        }
    }

    account_for_void_param(&params);

    return params;
}

static bool is_func_quals_end(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_SEMICOLON ||
           type == MIDLEXER_TOKENTYPE_L_CURLY ||
           type == MIDLEXER_TOKENTYPE_ASSIGN;
}

static void set_quals_flag(const struct MidLexer_Token *tok,
                           struct MidParser_FuncQuals *quals,
                           struct MidDiag_DiagVec *diags)
{
    switch (tok->type) {
    case MIDLEXER_TOKENTYPE_CONST:
        quals->is_const = true;
        break;

    case MIDLEXER_TOKENTYPE_VOLATILE:
        quals->is_volatile = true;
        break;

    case MIDLEXER_TOKENTYPE_BITWISE_AND:
        quals->lv_ref = true;
        break;

    case MIDLEXER_TOKENTYPE_LOGICAL_AND:
        quals->rv_ref = true;
        break;

    case MIDLEXER_TOKENTYPE_FINAL:
        quals->is_final = true;
        break;

    case MIDLEXER_TOKENTYPE_OVERRIDE:
        quals->is_override = true;
        break;

    default:
        MidGen_dynpush(diags, MidDiag_expected_token_err(
                               "qualifier", tok, MIDDIAG_ERR_UNEXPECTED_TOKEN));
        break;
    }
}

mid_isize MidParser_parse_func_quals(const struct MidLexer_Token *toks,
                                   mid_isize start,
                                   struct MidParser_FuncQuals *quals,
                                   struct MidDiag_DiagVec *diags)
{
    *quals = (struct MidParser_FuncQuals){};

    mid_isize i;
    for (i = start; !is_func_quals_end(toks[i].type); ++i) {
        set_quals_flag(&toks[i], quals, diags);
    }

    if (toks[i].type != MIDLEXER_TOKENTYPE_ASSIGN)
        return i;

    ++i;
    if (toks[i].type == MIDLEXER_TOKENTYPE_DELETE)
        quals->is_delete = true;
    else if (toks[i].type == MIDLEXER_TOKENTYPE_DEFAULT)
        quals->is_default = true;
    else
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("qualifier", &toks[i],
                                               MIDDIAG_ERR_UNEXPECTED_TOKEN));
    return i + 1;
}

static struct MidSema_Scope *create_scope(struct MidSema_Scope *scope,
                                          struct MidParser_ASTNode *node,
                                          struct MidParser_Allocators *allocs,
                                          enum MidSema_ScopeType type)
{
    struct MidSema_Scope *child;
    MidGen_bumpmalloc(&allocs->scope, &child);
    *child =
        (struct MidSema_Scope){.parent = scope, .node = node, .type = type};
    MidGen_dynpush(&scope->childs, child);

    return child;
}

static void add_func_def(struct MidParser_FuncDecl *func,
                         struct MidDiag_DiagVec *diags)
{
    if (!func->name)
        return;

    auto ident = MidParser_func_ident(func);
    if (ident->def)
        MidGen_dynpush(diags,
                    MidDiag_ident_redefined_err(func->name, func->def_start,
                                                MIDDIAG_ERR_BAD_IDENTIFIER));
    ident->def = MIDPARSER_GET_NODE(func);
}

static void copy_params_to_scope(struct MidParser_FuncDecl *src,
                                 struct MidSema_Scope *dest)
{
    for (mid_isize i = 0; i < src->param_scope->idents.len; ++i) {
        auto ident = &src->param_scope->idents.arr[i];
        assert(ident->type == MIDSEMA_IDENTTYPE_VAR);

        MidGen_dynpush(&dest->idents, MidSema_copy_var_ident(ident));
    }
}

static struct MidSema_Scope *
setup_def_scope(struct MidParser_FuncDecl *self,
                struct MidParser_Allocators *allocs,
                struct MidDiag_DiagVec *diags)
{
    auto def = &MidParser_func_ident(self)->func_info.def_scope;
    if (*def) {
        MidGen_dynpush(diags, MidDiag_ident_redefined_err(
                               self->name, MIDPARSER_GET_START(self),
                               MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    *def = create_scope(MidParser_func_parent(self), MIDPARSER_GET_NODE(self),
                        allocs, MIDSEMA_SCOPETYPE_FUNC);
    // necessary to make the function parameters visible in the func body
    copy_params_to_scope(self, *def);

    return *def;
}

mid_isize MidParser_parse_func_body(struct MidParser_FuncDecl *self,
                                  const struct MidLexer_Token *toks,
                                  mid_isize lcurly,
                                  struct MidParser_Allocators *allocs,
                                  struct MidDiag_DiagVec *diags)
{
    add_func_def(self, diags);

    mid_isize rcurly = MidParser_find_twin_curly(toks, lcurly, MID_ISIZE_MAX);
    if (rcurly == -1) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("'}'", &toks[lcurly],
                                               MIDDIAG_ERR_MISSING_CURLY));
        return lcurly + 1;
    }

    auto def = setup_def_scope(self, allocs, diags);

    for (mid_isize i = lcurly + 1; i < rcurly;) {
        auto child = MidParser_parse_node(
            toks, i, &i, MIDPARSER_GET_NODE(self), def,
            (struct MidParser_ParseNodeFlags){.skip_def = false}, allocs,
            diags);
        MidGen_dynpush(&self->nodes, child);
    }

    return rcurly;
}

// op is the index of the operator
// Type operator+(const Type &a, const Type &b)
//              ^
//              op
// NOTE: can't capture information dependent on other stuff, like whether an
//       increment overload is postfix or prefix. assumes postfix by default and
//       binary operators instead of unary operators by default for stuff like
//       '+' and '-'
static enum MidParser_ExprType
parse_operator_overload(const struct MidLexer_Token *toks, mid_isize op,
                        mid_isize *out_end, struct MidDiag_DiagVec *diags)
{
    if (out_end)
        *out_end = op + 1;

    switch (toks[op].type) {
    case MIDLEXER_TOKENTYPE_ADD:
        return MIDPARSER_EXPRTYPE_ADD;

    case MIDLEXER_TOKENTYPE_SUB:
        return MIDPARSER_EXPRTYPE_SUB;

    case MIDLEXER_TOKENTYPE_MUL:
        return MIDPARSER_EXPRTYPE_MUL;

    case MIDLEXER_TOKENTYPE_DIV:
        return MIDPARSER_EXPRTYPE_DIV;

    case MIDLEXER_TOKENTYPE_MOD:
        return MIDPARSER_EXPRTYPE_MOD;

    case MIDLEXER_TOKENTYPE_INC:
        return MIDPARSER_EXPRTYPE_POSTFIX_INC;

    case MIDLEXER_TOKENTYPE_DEC:
        return MIDPARSER_EXPRTYPE_POSTFIX_DEC;

    case MIDLEXER_TOKENTYPE_EQ:
        return MIDPARSER_EXPRTYPE_EQ;

    case MIDLEXER_TOKENTYPE_NEQ:
        return MIDPARSER_EXPRTYPE_NEQ;

    case MIDLEXER_TOKENTYPE_GT:
        return MIDPARSER_EXPRTYPE_GT;

    case MIDLEXER_TOKENTYPE_LT:
        return MIDPARSER_EXPRTYPE_LT;

    case MIDLEXER_TOKENTYPE_GTEQ:
        return MIDPARSER_EXPRTYPE_GTEQ;

    case MIDLEXER_TOKENTYPE_LTEQ:
        return MIDPARSER_EXPRTYPE_LTEQ;

    case MIDLEXER_TOKENTYPE_LOGICAL_NOT:
        return MIDPARSER_EXPRTYPE_LOGICAL_NOT;

    case MIDLEXER_TOKENTYPE_LOGICAL_AND:
        return MIDPARSER_EXPRTYPE_LOGICAL_AND;

    case MIDLEXER_TOKENTYPE_LOGICAL_OR:
        return MIDPARSER_EXPRTYPE_LOGICAL_OR;

    case MIDLEXER_TOKENTYPE_BITWISE_NOT:
        return MIDPARSER_EXPRTYPE_BITWISE_NOT;

    case MIDLEXER_TOKENTYPE_BITWISE_AND:
        return MIDPARSER_EXPRTYPE_BITWISE_AND;

    case MIDLEXER_TOKENTYPE_BITWISE_OR:
        return MIDPARSER_EXPRTYPE_BITWISE_OR;

    case MIDLEXER_TOKENTYPE_BITWISE_XOR:
        return MIDPARSER_EXPRTYPE_BITWISE_XOR;

    case MIDLEXER_TOKENTYPE_LEFT_SHIFT:
        return MIDPARSER_EXPRTYPE_LEFT_SHIFT;

    case MIDLEXER_TOKENTYPE_RIGHT_SHIFT:
        return MIDPARSER_EXPRTYPE_RIGHT_SHIFT;

    case MIDLEXER_TOKENTYPE_ASSIGN:
        return MIDPARSER_EXPRTYPE_ASSIGN;

    case MIDLEXER_TOKENTYPE_ADD_ASSIGN:
        return MIDPARSER_EXPRTYPE_ADD_ASSIGN;

    case MIDLEXER_TOKENTYPE_SUB_ASSIGN:
        return MIDPARSER_EXPRTYPE_SUB_ASSIGN;

    case MIDLEXER_TOKENTYPE_MUL_ASSIGN:
        return MIDPARSER_EXPRTYPE_MUL_ASSIGN;

    case MIDLEXER_TOKENTYPE_DIV_ASSIGN:
        return MIDPARSER_EXPRTYPE_DIV_ASSIGN;

    case MIDLEXER_TOKENTYPE_MOD_ASSIGN:
        return MIDPARSER_EXPRTYPE_MOD_ASSIGN;

    case MIDLEXER_TOKENTYPE_AND_ASSIGN:
        return MIDPARSER_EXPRTYPE_AND_ASSIGN;

    case MIDLEXER_TOKENTYPE_OR_ASSIGN:
        return MIDPARSER_EXPRTYPE_OR_ASSIGN;

    case MIDLEXER_TOKENTYPE_XOR_ASSIGN:
        return MIDPARSER_EXPRTYPE_XOR_ASSIGN;

    case MIDLEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        return MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;

    case MIDLEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        return MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;

    case MIDLEXER_TOKENTYPE_L_SQBRACKET:
        if (toks[op + 1].type != MIDLEXER_TOKENTYPE_R_SQBRACKET)
            MidGen_dynpush(
                diags, MidDiag_expected_token_err("]", &toks[op],
                                                  MIDDIAG_ERR_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return MIDPARSER_EXPRTYPE_ARRAY_SUBSCR;

    case MIDLEXER_TOKENTYPE_PTR_MEMB_SEL:
        return MIDPARSER_EXPRTYPE_PTR_MEMB_SEL;

    case MIDLEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        return MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;

    case MIDLEXER_TOKENTYPE_L_PAREN:
        if (toks[op + 1].type != MIDLEXER_TOKENTYPE_R_PAREN)
            MidGen_dynpush(
                diags, MidDiag_expected_token_err(")", &toks[op],
                                                  MIDDIAG_ERR_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return MIDPARSER_EXPRTYPE_FUNC_CALL;

    case MIDLEXER_TOKENTYPE_COMMA:
        return MIDPARSER_EXPRTYPE_COMMA;

    case MIDLEXER_TOKENTYPE_NEW:
        if (toks[op + 1].type == MIDLEXER_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (toks[op + 2].type != MIDLEXER_TOKENTYPE_R_SQBRACKET)
                MidGen_dynpush(diags, MidDiag_expected_token_err(
                                       "]", &toks[op + 1],
                                       MIDDIAG_ERR_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return MIDPARSER_EXPRTYPE_NEW_ARR;
        } else {
            return MIDPARSER_EXPRTYPE_NEW;
        }

    case MIDLEXER_TOKENTYPE_DELETE:
        if (toks[op + 1].type == MIDLEXER_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (toks[op + 2].type != MIDLEXER_TOKENTYPE_R_SQBRACKET)
                MidGen_dynpush(diags, MidDiag_expected_token_err(
                                       "]", &toks[op + 1],
                                       MIDDIAG_ERR_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return MIDPARSER_EXPRTYPE_DELETE_ARR;
        } else {
            return MIDPARSER_EXPRTYPE_DELETE;
        }

    default:
        MidGen_dynpush(diags, ((struct MidDiag_Diag){
                               .pos = toks[op].pos,
                               .line = toks[op].line,
                               .msg = strdup("can't overload operator"),
                               .err = MIDDIAG_ERR_BAD_OP_OVERLOAD,
                               .type = MIDDIAG_TYPE_ERROR,
                           }));
        if (out_end)
            --*out_end;
        // just default to add for now
        return MIDPARSER_EXPRTYPE_ADD;
    }
}

static mid_isize parse_func_type(struct MidParser_FuncDecl *self,
                               const struct MidLexer_Token *toks, mid_isize start,
                               struct MidSema_Scope *parent_scope,
                               struct MidSema_Scope **out_res,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags)
{
    mid_isize type_end;
    mid_isize name;
    self->ret = MidParser_parse_type(toks, start, &type_end, parent_scope,
                                     &name, false, allocs, diags);

    auto res = name == -1 ? parent_scope
                          : MidParser_parse_scope_res(toks, name, &name,
                                                      parent_scope, diags);
    if (out_res)
        *out_res = res;

    self->name = name == -1 || toks[name].type != MIDLEXER_TOKENTYPE_IDENTIFIER
                     ? NULL
                     : toks[name].ident;

    if (!self->name) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("identifier", &toks[start],
                                               MIDDIAG_ERR_MISSING_TOKEN));
        self->name = "INVALID-FUNC-NAME";
    } else if (!strcmp(self->name, "operator")) {
        self->is_op_overload = true;
        self->op_overload =
            parse_operator_overload(toks, type_end, &type_end, diags);
    }

    return type_end;
}

static struct MidParser_Class *find_tor_class(struct MidSema_Scope *scope,
                                              const char *name)
{
    if (scope->type == MIDSEMA_SCOPETYPE_CLASS) {
        if (!strcmp(scope->node->class_.name, name))
            return &scope->node->class_;
    }

    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (ident->type != MIDSEMA_IDENTTYPE_CLASS)
            continue;

        if (!strcmp(ident->name, name))
            return &ident->decl->class_;
    }

    return NULL;
}

static mid_isize parse_tor_type(struct MidParser_FuncDecl *self,
                              const struct MidLexer_Token *toks, mid_isize start,
                              struct MidParser_Class **out_class,
                              struct MidSema_Scope *parent_scope,
                              struct MidDiag_DiagVec *diags)
{
    mid_isize name_idx;
    auto res =
        MidParser_parse_scope_res(toks, start, &name_idx, parent_scope, diags);

    self->is_dtor = toks[name_idx].type == MIDLEXER_TOKENTYPE_BITWISE_NOT;
    if (self->is_dtor)
        ++name_idx;

    assert(toks[name_idx].type == MIDLEXER_TOKENTYPE_IDENTIFIER);
    auto class_ = find_tor_class(res, toks[name_idx].ident);
    assert(class_);
    if (out_class)
        *out_class = class_;
    self->ret = MidSema_node_type(MIDPARSER_GET_NODE(class_), res);

    return name_idx + 1;
}

// some operator overloads are ambiguous until the parameters have been parsed
static void disambig_operator_overload(struct MidParser_FuncDecl *self)
{
    assert(self->is_op_overload);

    bool implicit_this =
        MidParser_func_parent(self)->type == MIDSEMA_SCOPETYPE_CLASS;
    mid_isize n_params = self->params.len + implicit_this;
    if (n_params == 1) {
        switch (self->op_overload) {
        case MIDPARSER_EXPRTYPE_ADD:
            self->op_overload = MIDPARSER_EXPRTYPE_UNARY_PLUS;
            break;

        case MIDPARSER_EXPRTYPE_SUB:
            self->op_overload = MIDPARSER_EXPRTYPE_UNARY_MINUS;
            break;

        case MIDPARSER_EXPRTYPE_MUL:
            self->op_overload = MIDPARSER_EXPRTYPE_DEREF;
            break;

        case MIDPARSER_EXPRTYPE_BITWISE_AND:
            self->op_overload = MIDPARSER_EXPRTYPE_REF;
            break;

        case MIDPARSER_EXPRTYPE_POSTFIX_INC:
            self->op_overload = MIDPARSER_EXPRTYPE_PREFIX_INC;
            break;

        case MIDPARSER_EXPRTYPE_POSTFIX_DEC:
            self->op_overload = MIDPARSER_EXPRTYPE_PREFIX_DEC;
            break;

        default:
            break;
        }
    }
}

static void add_func_to_scope(struct MidSema_Scope *scope,
                              struct MidParser_FuncDecl *self)
{
    bool is_tmplt = MidParser_node_is_templated(MIDPARSER_GET_NODE(self));
    enum MidSema_IdentType type =
        is_tmplt ? MIDSEMA_IDENTTYPE_TMPLT_FUNC : MIDSEMA_IDENTTYPE_FUNC;

    const struct MidSema_Ident *old = MidSema_add_ident(
        scope, &(struct MidSema_Ident){.name = self->name,
                                       .decl = MIDPARSER_GET_NODE(self),
                                       .type = type});

    if (old)
        self->ident_idx = old - scope->idents.arr;
    else
        self->ident_idx = scope->idents.len - 1;
}

// default_args can't be const cuz of sum discarding qualifiers in nested ptrs
// stuff idk
static bool missing_default_args(struct MidParser_Expr **default_args,
                                 mid_isize n, mid_isize *out_bad_idx)
{
    bool found_default = false;
    for (mid_isize i = 0; i < n; ++i) {
        if (default_args[i])
            found_default = true;
        else if (found_default) {
            if (out_bad_idx)
                *out_bad_idx = i;
            return true;
        }
    }

    return false;
}

static void register_default_args(struct MidParser_FuncDecl *decl,
                                  struct MidDiag_DiagVec *diags)
{
    auto default_args = &MidParser_func_ident(decl)->func_info.default_args;
    if (!*default_args)
        *default_args = Mid_calloc(decl->params.len, sizeof(**default_args));

    for (mid_isize i = 0; i < decl->params.len; ++i) {
        auto default_arg = (*default_args)[i];

        auto node = (struct MidParser_ASTNode *)decl->params.arr[i];
        auto param = node->var_decl.insts.arr[0];
        if (!param->init.expr) // not a default arg
            continue;

        if (default_arg) {
            MidGen_dynpush(diags, MidDiag_ident_redefined_err(
                                   param->name, node->start,
                                   MIDDIAG_ERR_BAD_DEFAULT_ARGUMENT));
            continue;
        }

        (*default_args)[i] = param->init.expr;
    }

    mid_isize bad;
    if (missing_default_args(*default_args, decl->params.len, &bad))
        MidGen_dynpush(diags,
                    missing_default_arg_err(
                        decl->name, MIDPARSER_GET_START(decl->params.arr[bad]),
                        MIDDIAG_ERR_BAD_DEFAULT_ARGUMENT));
}

mid_isize MidParser_parse_func_decl(
    struct MidParser_FuncDecl *self, const struct MidLexer_Token *toks,
    mid_isize start, struct MidSema_Scope *parent_scope, bool skip_def,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_FuncDecl){.ident_idx = -1};

    struct MidSema_Scope *res;
    mid_isize type_end =
        parse_func_type(self, toks, start, parent_scope, &res, allocs, diags);
    if (toks[type_end].type != MIDLEXER_TOKENTYPE_L_PAREN)
        MID_CRASH("function missing left paren");

    self->param_scope = create_scope(res, MIDPARSER_GET_NODE(self), allocs,
                                     MIDSEMA_SCOPETYPE_FUNC_PARAMS);

    mid_isize lparen = type_end;
    mid_isize rparen;
    self->params = MidParser_parse_func_params(
        toks, lparen, &rparen, MIDPARSER_GET_NODE(self), self->param_scope,
        true, &self->variadic, allocs, diags);
    mid_isize lcurly =
        MidParser_parse_func_quals(toks, rparen + 1, &self->quals, diags);
    if (self->is_op_overload)
        disambig_operator_overload(self);
    if (self->name)
        add_func_to_scope(res, self);
    register_default_args(self, diags);

    if (toks[lcurly].type != MIDLEXER_TOKENTYPE_L_CURLY)
        return lcurly;
    self->def_start = &toks[lcurly];
    self->has_def = true;

    if (skip_def) {
        mid_isize rcurly = MidParser_find_twin_curly(toks, lcurly, MID_ISIZE_MAX);
        return rcurly == -1 ? lcurly + 1 : rcurly + 1;
    } else {
        mid_isize rcurly =
            MidParser_parse_func_body(self, toks, lcurly, allocs, diags);
        return rcurly + 1;
    }
}

mid_isize MidParser_parse_tor(struct MidParser_FuncDecl *self,
                            const struct MidLexer_Token *toks, mid_isize start,
                            struct MidSema_Scope *parent_scope, bool skip_def,
                            struct MidParser_Allocators *allocs,
                            struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_FuncDecl){.ident_idx = -1, .is_tor = true};

    struct MidParser_Class *class_;
    mid_isize lparen =
        parse_tor_type(self, toks, start, &class_, parent_scope, diags);

    self->name = self->is_dtor ? MidParser_dtor_name : MidParser_ctor_name;

    auto c_scope = MidSema_deref_identptr(&class_->ident)->class_info.def_scope;
    self->param_scope = create_scope(c_scope, MIDPARSER_GET_NODE(self), allocs,
                                     MIDSEMA_SCOPETYPE_FUNC_PARAMS);

    mid_isize rparen;
    self->params = MidParser_parse_func_params(
        toks, lparen, &rparen, MIDPARSER_GET_NODE(self), self->param_scope,
        true, &self->variadic, allocs, diags);
    mid_isize lcurly =
        MidParser_parse_func_quals(toks, rparen + 1, &self->quals, diags);
    add_func_to_scope(c_scope, self);
    register_default_args(self, diags);

    if (toks[lcurly].type != MIDLEXER_TOKENTYPE_L_CURLY)
        return lcurly;
    self->def_start = &toks[lcurly];
    self->has_def = true;

    if (skip_def) {
        mid_isize rcurly = MidParser_find_twin_curly(toks, lcurly, MID_ISIZE_MAX);
        return rcurly == -1 ? lcurly + 1 : rcurly + 1;
    } else {
        mid_isize rcurly =
            MidParser_parse_func_body(self, toks, lcurly, allocs, diags);
        return rcurly + 1;
    }
}

bool MidParser_func_is_method(const struct MidParser_FuncDecl *self)
{
    return MidParser_func_parent(self)->type == MIDSEMA_SCOPETYPE_CLASS;
}

bool MidParser_func_is_ctor(const struct MidParser_FuncDecl *self)
{
    return self->is_tor && !self->is_dtor;
}

bool MidParser_func_takes_implicit_this(const struct MidParser_FuncDecl *self,
                                        bool cnt_ctors)
{
    if (cnt_ctors && MidParser_func_is_ctor(self))
        return true;
    return MidParser_func_is_method(self) && !MidParser_func_is_ctor(self) &&
           !self->ret.squals.is_static;
}

struct MidParser_Type
MidParser_implicit_this_type(const struct MidParser_FuncDecl *self)
{
    const struct MidSema_Scope *parent = MidParser_func_parent(self);
    return MidSema_node_type(parent->node, parent->parent);
}

bool MidParser_func_is_main(const struct MidParser_FuncDecl *self)
{
    return self->param_scope->parent->type == MIDSEMA_SCOPETYPE_ROOT &&
           !strcmp(self->name, "main");
}
