#include "parser/func_decl.h"
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
#include "parser/class.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include "sema/typecheck.h"
#include <string.h>

static struct mid_Diag missing_default_arg_err(const char *func,
                                               const struct midlex_Token *tok,
                                               enum middiag_ErrT err_type)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg =
            midcmd_fmt_to_str("function '%s' missing default argument", func),
        .err = err_type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

void midpar_CtorMemberInit_deinit(struct midpar_CtorMemberInit *self)
{
    for (mid_isize i = 0; i < self->n_args; ++i) {
        midpar_Expr_deinit(&self->args[i]);
    }
    free(self->args);
}

void midpar_FuncDecl_deinit(struct midpar_FuncDecl *self)
{
    midgen_dyndeinit(&self->nodes);
    midgen_dyndeinit(&self->params);
    midgen_dyndeinit(&self->memb_inits);
    midpar_Type_deinit(&self->ret);
}

void midpar_copy_ctor_memb_init(struct midpar_CtorMemberInit *dest,
                                const struct midpar_CtorMemberInit *src)
{
    *dest = *src;

    dest->args = mid_malloc(src->n_args * sizeof(*dest->args));
    for (mid_isize i = 0; i < src->n_args; ++i)
        dest->args[i] = midpar_copy_expr(&src->args[i]);
}

void midpar_copy_func_decl(struct midpar_FuncDecl *dest,
                           const struct midpar_FuncDecl *src,
                           struct midsema_Scope *dest_scope,
                           struct midpar_Allocators *allocs)
{
    *dest = *src;

    auto old_ident = midsema_add_ident_copy(dest_scope, midpar_func_ident(src),
                                            false, allocs);
    if (old_ident)
        dest->ident_idx = old_ident - dest_scope->idents.arr;
    else
        dest->ident_idx = dest_scope->idents.len - 1;

    dest->ret = midpar_copy_type(&src->ret);

    midgen_bumpmalloc(&allocs->scope, &dest->param_scope);
    *dest->param_scope = midsema_create_empty_scope(
        src->param_scope->type, dest_scope, MIDPAR_GET_NODE(dest));

    auto params_nodes =
        midpar_copy_nodepvec((struct midpar_ASTNodePVec *)&src->params,
                             MIDPAR_GET_NODE(dest), dest->param_scope, allocs);
    dest->params = (struct midpar_VarDeclPVec){.arr = (void *)params_nodes.arr,
                                               .len = params_nodes.len,
                                               .cap = params_nodes.cap};

    if (src->nodes.len > 0) {
        struct midsema_Scope *def_scope;
        midgen_bumpmalloc(&allocs->scope, &def_scope);
        *def_scope = (struct midsema_Scope){.parent = dest_scope,
                                            .node = MIDPAR_GET_NODE(dest),
                                            .type = MIDSEMA_SCOPETYPE_FUNC};
        midpar_func_ident(dest)->func_info.def_scope = def_scope;
        dest->nodes = midpar_copy_nodepvec(&src->nodes, MIDPAR_GET_NODE(dest),
                                           def_scope, allocs);
    }
}

struct midsema_Scope *midpar_func_parent(const struct midpar_FuncDecl *func)
{
    auto ret = func->param_scope->parent;
    assert(ret);
    return ret;
}

struct midsema_Ident *midpar_func_ident(const struct midpar_FuncDecl *func)
{
    assert(func->ident_idx != -1);
    auto parent = midpar_func_parent(func);
    return &parent->idents.arr[func->ident_idx];
}

// accounts for functions taking a singular void parameter
// example: int f(void)
static void account_for_void_param(struct midpar_VarDeclPVec *params)
{
    if (params->len == 1 &&
        midsema_type_is_void(&params->arr[0]->insts.arr[0]->type)) {
        midgen_dyndeinit(params);
    }
}

struct midpar_VarDeclPVec
midpar_parse_func_params(midlex_TokenIter lparen, midlex_TokenIter *out_rparen,
                         struct midpar_ASTNode *parent,
                         struct midsema_Scope *scope, bool add_to_scope,
                         bool *out_variadic, struct midpar_Allocators *allocs,
                         struct mid_DiagVec *diags)
{
    struct midpar_VarDeclPVec params = {};

    midlex_TokenIter rparen = midpar_find_twin_paren(lparen, nullptr);
    if (out_rparen)
        *out_rparen = rparen;

    if (!rparen) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "')'", lparen, MIDDIAG_ERR_MISSING_PAREN));
        return params;
    }

    if (out_variadic)
        *out_variadic = false;

    for (midlex_TokenIter i = lparen + 1; i < rparen; ++i) {
        if (i->type == MIDLEX_TOKENTYPE_ELLIPSIS) {
            if (out_variadic)
                *out_variadic = true;
            if (i + 1 < rparen)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "')'", lparen, MIDDIAG_ERR_MISSING_PAREN));
            break;
        } else {
            struct midpar_ASTNode *child;
            midgen_bumpmalloc(&allocs->ast, &child);
            *child =
                (struct midpar_ASTNode){.parent = parent,
                                        .start = i,
                                        .type = MIDPAR_ASTNODETYPE_VAR_DECL};
            i = midpar_parse_var_decl(
                &child->var_decl, i, MIDPAR_PARAM_ENDTYPES,
                (struct midpar_ParseVarDeclFlags){.add_to_scope = add_to_scope,
                                                  .single_inst = true},
                scope, allocs, diags);
            midgen_dynpush(&params, (void *)child);
        }
    }

    account_for_void_param(&params);

    return params;
}

static bool is_func_quals_end(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_SEMICOLON ||
           type == MIDLEX_TOKENTYPE_COLON || type == MIDLEX_TOKENTYPE_L_CURLY ||
           type == MIDLEX_TOKENTYPE_ASSIGN;
}

static void set_quals_flag(const struct midlex_Token *tok,
                           struct midpar_FuncQuals *quals,
                           struct mid_DiagVec *diags)
{
    switch (tok->type) {
    case MIDLEX_TOKENTYPE_CONST:
        quals->is_const = true;
        break;

    case MIDLEX_TOKENTYPE_VOLATILE:
        quals->is_volatile = true;
        break;

    case MIDLEX_TOKENTYPE_BITWISE_AND:
        quals->lv_ref = true;
        break;

    case MIDLEX_TOKENTYPE_LOGICAL_AND:
        quals->rv_ref = true;
        break;

    case MIDLEX_TOKENTYPE_FINAL:
        quals->is_final = true;
        break;

    case MIDLEX_TOKENTYPE_OVERRIDE:
        quals->is_override = true;
        break;

    default:
        midgen_dynpush(
            diags, middiag_expected_token_err("qualifier", tok,
                                              MIDDIAG_ERR_UNEXPECTED_TOKEN));
        break;
    }
}

// if init is NULL then the initializer is skipped
static midlex_TokenIter
parse_ctor_init_exprs(struct midpar_CtorMemberInit *init,
                      struct midsema_Scope *scope, midlex_TokenIter lparen,
                      struct mid_DiagVec *diags)
{
    struct midpar_ExprVec exprs = {};

    midlex_TokenIter i;
    for (i = lparen + 1; i->type != MIDLEX_TOKENTYPE_R_PAREN; ++i) {
        if (init)
            midgen_dynpush(&exprs, midpar_parse_expr(i, MIDPAR_ARG_ENDTYPES, &i,
                                                     scope, diags));
        else
            i = midpar_skip_expr(i, MIDPAR_ARG_ENDTYPES, diags);

        if (i->type == MIDLEX_TOKENTYPE_COMMA)
            continue;
        else if (i->type != MIDLEX_TOKENTYPE_R_PAREN)
            midgen_dynpush(diags, middiag_expected_token_err(
                                      ")", i - 1, MIDDIAG_ERR_MISSING_PAREN));
        break;
    }

    if (init) {
        init->args = exprs.arr;
        init->n_args = exprs.len;
    }

    if (i->type == MIDLEX_TOKENTYPE_R_PAREN)
        return i + 1;
    else
        return i;
}

// if self is NULL the initializer is skipped
static midlex_TokenIter parse_ctor_init(struct midpar_FuncDecl *self,
                                        struct midsema_Scope *scope,
                                        midlex_TokenIter start,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags)
{
    // a ctor initializer goes like: a({expr}, {expr}, ...)

    assert(start->type == MIDLEX_TOKENTYPE_IDENTIFIER);

    midlex_TokenIter lparen = start + 1;
    if (lparen->type != MIDLEX_TOKENTYPE_L_PAREN) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "(", start, MIDDIAG_ERR_MISSING_PAREN));
        return lparen;
    }

    midlex_TokenIter end;
    if (self) {
        struct midpar_ASTNode *node;
        midgen_bumpmalloc(&allocs->ast, &node);
        node->parent = MIDPAR_GET_NODE(self);
        node->start = start;
        node->type = MIDPAR_ASTNODETYPE_CTOR_MEMB_INIT;

        struct midpar_CtorMemberInit *init = &node->memb_init;
        init->name = start->ident;
        end = parse_ctor_init_exprs(init, scope, lparen, diags);

        midgen_dynpush(&self->memb_inits, init);
    } else {
        end = parse_ctor_init_exprs(nullptr, scope, lparen, diags);
    }

    return end;
}

// if self is NULL the initializer list is simply skipped
static midlex_TokenIter parse_ctor_init_list(struct midpar_FuncDecl *self,
                                             struct midsema_Scope *scope,
                                             midlex_TokenIter start,
                                             struct midpar_Allocators *allocs,
                                             struct mid_DiagVec *diags)
{
    // initializer lists go like: Class(...) : a(1, 2, 3), b(1, 2, 3), ... {}
    //                                       ^
    //                                     start

    assert(start->type == MIDLEX_TOKENTYPE_COLON);

    midlex_TokenIter i;
    for (i = start + 1;; ++i) {
        i = parse_ctor_init(self, scope, i, allocs, diags);

        if (i->type == MIDLEX_TOKENTYPE_COMMA)
            continue;
        else if (i->type != MIDLEX_TOKENTYPE_L_CURLY &&
                 i->type != MIDLEX_TOKENTYPE_SEMICOLON)
            midgen_dynpush(diags, middiag_expected_token_err(
                                      "{", i - 1, MIDDIAG_ERR_MISSING_PAREN));
        break;
    }

    return i;
}

midlex_TokenIter parse_func_quals(midlex_TokenIter start,
                                  struct midpar_FuncQuals *quals,
                                  bool is_constexpr, struct mid_DiagVec *diags)
{
    *quals = (struct midpar_FuncQuals){.is_constexpr = is_constexpr};

    midlex_TokenIter i;
    for (i = start; !is_func_quals_end(i->type); ++i) {
        set_quals_flag(i, quals, diags);
    }

    if (i->type != MIDLEX_TOKENTYPE_ASSIGN)
        return i;

    ++i;
    if (i->type == MIDLEX_TOKENTYPE_DELETE)
        quals->is_delete = true;
    else if (i->type == MIDLEX_TOKENTYPE_DEFAULT)
        quals->is_default = true;
    else
        midgen_dynpush(
            diags, middiag_expected_token_err("qualifier", i,
                                              MIDDIAG_ERR_UNEXPECTED_TOKEN));
    return i + 1;
}

static struct midsema_Scope *create_scope(struct midsema_Scope *scope,
                                          struct midpar_ASTNode *node,
                                          struct midpar_Allocators *allocs,
                                          enum midsema_ScopeType type)
{
    struct midsema_Scope *child;
    midgen_bumpmalloc(&allocs->scope, &child);
    *child =
        (struct midsema_Scope){.parent = scope, .node = node, .type = type};
    midgen_dynpush(&scope->childs, child);

    return child;
}

static void add_func_def(struct midpar_FuncDecl *func,
                         struct mid_DiagVec *diags)
{
    if (!func->name)
        return;

    auto ident = midpar_func_ident(func);
    if (ident->def)
        midgen_dynpush(diags,
                       middiag_ident_redefined_err(func->name, func->def_start,
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));
    ident->def = MIDPAR_GET_NODE(func);
}

static void copy_params_to_scope(struct midpar_FuncDecl *src,
                                 struct midsema_Scope *dest)
{
    for (mid_isize i = 0; i < src->param_scope->idents.len; ++i) {
        auto ident = &src->param_scope->idents.arr[i];
        assert(ident->type == MIDSEMA_IDENTTYPE_VAR);

        midgen_dynpush(&dest->idents, midsema_copy_var_ident(ident));
    }
}

static struct midsema_Scope *setup_def_scope(struct midpar_FuncDecl *self,
                                             struct midpar_Allocators *allocs,
                                             struct mid_DiagVec *diags)
{
    auto def = &midpar_func_ident(self)->func_info.def_scope;
    if (*def) {
        midgen_dynpush(diags, middiag_ident_redefined_err(
                                  self->name, MIDPAR_GET_START(self),
                                  MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    *def = create_scope(midpar_func_parent(self), MIDPAR_GET_NODE(self), allocs,
                        MIDSEMA_SCOPETYPE_FUNC);
    // necessary to make the function parameters visible in the func body
    copy_params_to_scope(self, *def);

    return *def;
}

static midlex_TokenIter parse_default_or_delete(struct midpar_FuncDecl *self,
                                                midlex_TokenIter start,
                                                struct mid_DiagVec *diags)
{
    if (start->type == MIDLEX_TOKENTYPE_DEFAULT)
        self->quals.is_default = true;
    else if (start->type == MIDLEX_TOKENTYPE_DELETE)
        self->quals.is_delete = true;
    else
        midgen_dynpush(
            diags, middiag_expected_token_err("default", start,
                                              MIDDIAG_ERR_UNEXPECTED_TOKEN));

    return start + 1;
}

midlex_TokenIter midpar_parse_func_body(struct midpar_FuncDecl *self,
                                        midlex_TokenIter start,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags)
{
    add_func_def(self, diags);

    if (start->type == MIDLEX_TOKENTYPE_COLON)
        start = parse_ctor_init_list(self, self->param_scope->parent, start,
                                     allocs, diags);

    if (start->type == MIDLEX_TOKENTYPE_ASSIGN) {
        return parse_default_or_delete(self, start, diags);
    }

    auto lcurly = start;
    auto rcurly = midpar_find_twin_curly(lcurly, nullptr);
    if (!rcurly) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "'}'", lcurly, MIDDIAG_ERR_MISSING_CURLY));
        return lcurly + 1;
    }

    auto def = setup_def_scope(self, allocs, diags);

    for (auto i = lcurly + 1; i < rcurly;) {
        auto child = midpar_parse_node(
            i, &i, MIDPAR_GET_NODE(self), def,
            (struct midpar_ParseNodeFlags){.skip_def = false}, allocs, diags);
        midgen_dynpush(&self->nodes, child);
    }

    midsema_typecheck_func_body(self, diags);

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
static enum midpar_ExprType parse_operator_overload(midlex_TokenIter op,
                                                    midlex_TokenIter *out_end,
                                                    struct mid_DiagVec *diags)
{
    if (out_end)
        *out_end = op + 1;

    midlex_TokenIter next = op + 1;
    midlex_TokenIter next2 = next + 1;

    switch (op->type) {
    case MIDLEX_TOKENTYPE_ADD:
        return MIDPAR_EXPRTYPE_ADD;

    case MIDLEX_TOKENTYPE_SUB:
        return MIDPAR_EXPRTYPE_SUB;

    case MIDLEX_TOKENTYPE_MUL:
        return MIDPAR_EXPRTYPE_MUL;

    case MIDLEX_TOKENTYPE_DIV:
        return MIDPAR_EXPRTYPE_DIV;

    case MIDLEX_TOKENTYPE_MOD:
        return MIDPAR_EXPRTYPE_MOD;

    case MIDLEX_TOKENTYPE_INC:
        return MIDPAR_EXPRTYPE_POSTFIX_INC;

    case MIDLEX_TOKENTYPE_DEC:
        return MIDPAR_EXPRTYPE_POSTFIX_DEC;

    case MIDLEX_TOKENTYPE_EQ:
        return MIDPAR_EXPRTYPE_EQ;

    case MIDLEX_TOKENTYPE_NEQ:
        return MIDPAR_EXPRTYPE_NEQ;

    case MIDLEX_TOKENTYPE_GT:
        return MIDPAR_EXPRTYPE_GT;

    case MIDLEX_TOKENTYPE_LT:
        return MIDPAR_EXPRTYPE_LT;

    case MIDLEX_TOKENTYPE_GTEQ:
        return MIDPAR_EXPRTYPE_GTEQ;

    case MIDLEX_TOKENTYPE_LTEQ:
        return MIDPAR_EXPRTYPE_LTEQ;

    case MIDLEX_TOKENTYPE_LOGICAL_NOT:
        return MIDPAR_EXPRTYPE_LOGICAL_NOT;

    case MIDLEX_TOKENTYPE_LOGICAL_AND:
        return MIDPAR_EXPRTYPE_LOGICAL_AND;

    case MIDLEX_TOKENTYPE_LOGICAL_OR:
        return MIDPAR_EXPRTYPE_LOGICAL_OR;

    case MIDLEX_TOKENTYPE_BITWISE_NOT:
        return MIDPAR_EXPRTYPE_BITWISE_NOT;

    case MIDLEX_TOKENTYPE_BITWISE_AND:
        return MIDPAR_EXPRTYPE_BITWISE_AND;

    case MIDLEX_TOKENTYPE_BITWISE_OR:
        return MIDPAR_EXPRTYPE_BITWISE_OR;

    case MIDLEX_TOKENTYPE_BITWISE_XOR:
        return MIDPAR_EXPRTYPE_BITWISE_XOR;

    case MIDLEX_TOKENTYPE_LEFT_SHIFT:
        return MIDPAR_EXPRTYPE_LEFT_SHIFT;

    case MIDLEX_TOKENTYPE_RIGHT_SHIFT:
        return MIDPAR_EXPRTYPE_RIGHT_SHIFT;

    case MIDLEX_TOKENTYPE_ASSIGN:
        return MIDPAR_EXPRTYPE_ASSIGN;

    case MIDLEX_TOKENTYPE_ADD_ASSIGN:
        return MIDPAR_EXPRTYPE_ADD_ASSIGN;

    case MIDLEX_TOKENTYPE_SUB_ASSIGN:
        return MIDPAR_EXPRTYPE_SUB_ASSIGN;

    case MIDLEX_TOKENTYPE_MUL_ASSIGN:
        return MIDPAR_EXPRTYPE_MUL_ASSIGN;

    case MIDLEX_TOKENTYPE_DIV_ASSIGN:
        return MIDPAR_EXPRTYPE_DIV_ASSIGN;

    case MIDLEX_TOKENTYPE_MOD_ASSIGN:
        return MIDPAR_EXPRTYPE_MOD_ASSIGN;

    case MIDLEX_TOKENTYPE_AND_ASSIGN:
        return MIDPAR_EXPRTYPE_AND_ASSIGN;

    case MIDLEX_TOKENTYPE_OR_ASSIGN:
        return MIDPAR_EXPRTYPE_OR_ASSIGN;

    case MIDLEX_TOKENTYPE_XOR_ASSIGN:
        return MIDPAR_EXPRTYPE_XOR_ASSIGN;

    case MIDLEX_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        return MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN;

    case MIDLEX_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        return MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN;

    case MIDLEX_TOKENTYPE_L_SQBRACKET:
        if (next->type != MIDLEX_TOKENTYPE_R_SQBRACKET)
            midgen_dynpush(diags, middiag_expected_token_err(
                                      "]", op, MIDDIAG_ERR_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return MIDPAR_EXPRTYPE_ARRAY_SUBSCR;

    case MIDLEX_TOKENTYPE_PTR_MEMB_SEL:
        return MIDPAR_EXPRTYPE_PTR_MEMB_SEL;

    case MIDLEX_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        return MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;

    case MIDLEX_TOKENTYPE_L_PAREN:
        if (next->type != MIDLEX_TOKENTYPE_R_PAREN)
            midgen_dynpush(diags, middiag_expected_token_err(
                                      ")", op, MIDDIAG_ERR_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return MIDPAR_EXPRTYPE_FUNC_CALL;

    case MIDLEX_TOKENTYPE_COMMA:
        return MIDPAR_EXPRTYPE_COMMA;

    case MIDLEX_TOKENTYPE_NEW:
        if (next->type == MIDLEX_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (next2->type != MIDLEX_TOKENTYPE_R_SQBRACKET)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "]", next, MIDDIAG_ERR_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return MIDPAR_EXPRTYPE_NEW_ARR;
        } else {
            return MIDPAR_EXPRTYPE_NEW;
        }

    case MIDLEX_TOKENTYPE_DELETE:
        if (next->type == MIDLEX_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (next2->type != MIDLEX_TOKENTYPE_R_SQBRACKET)
                midgen_dynpush(diags,
                               middiag_expected_token_err(
                                   "]", next, MIDDIAG_ERR_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return MIDPAR_EXPRTYPE_DELETE_ARR;
        } else {
            return MIDPAR_EXPRTYPE_DELETE;
        }

    default:
        midgen_dynpush(diags, ((struct mid_Diag){
                                  .pos = op->pos,
                                  .line = op->line,
                                  .msg = strdup("can't overload operator"),
                                  .err = MIDDIAG_ERR_BAD_OP_OVERLOAD,
                                  .type = MIDDIAG_TYPE_ERROR,
                              }));
        if (out_end)
            --*out_end;
        // just default to add for now
        return MIDPAR_EXPRTYPE_ADD;
    }
}

static midlex_TokenIter parse_func_type(struct midpar_FuncDecl *self,
                                        midlex_TokenIter start,
                                        struct midsema_Scope *parent_scope,
                                        struct midsema_Scope **out_res,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags)
{
    midlex_TokenIter type_end;
    midlex_TokenIter name;
    self->ret = midpar_parse_type(start, &type_end, parent_scope, &name, false,
                                  allocs, diags);

    struct midsema_Scope *res =
        !name ? parent_scope
              : midpar_parse_scope_res(name, &name, parent_scope, diags);
    if (out_res)
        *out_res = res;

    self->name = !name || name->type != MIDLEX_TOKENTYPE_IDENTIFIER
                     ? nullptr
                     : name->ident;

    if (!self->name) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("identifier", start,
                                                  MIDDIAG_ERR_MISSING_TOKEN));
        self->name = "INVALID-FUNC-NAME";
    } else if (!strcmp(self->name, "operator")) {
        self->is_op_overload = true;
        self->op_overload = parse_operator_overload(type_end, &type_end, diags);
    }

    return type_end;
}

static struct midpar_Class *find_tor_class(struct midsema_Scope *scope,
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

static midlex_TokenIter parse_tor_type(struct midpar_FuncDecl *self,
                                       midlex_TokenIter start,
                                       struct midpar_Class **out_class,
                                       struct midsema_Scope *parent_scope,
                                       struct mid_DiagVec *diags)
{
    if (start->type == MIDLEX_TOKENTYPE_CONSTEXPR) {
        ++start;
        self->quals.is_constexpr = true;
    }

    midlex_TokenIter name_idx;
    auto res = midpar_parse_scope_res(start, &name_idx, parent_scope, diags);

    self->is_dtor = name_idx->type == MIDLEX_TOKENTYPE_BITWISE_NOT;
    if (self->is_dtor)
        ++name_idx;

    assert(name_idx->type == MIDLEX_TOKENTYPE_IDENTIFIER);
    auto class_ = find_tor_class(res, name_idx->ident);
    assert(class_);
    if (out_class)
        *out_class = class_;
    self->ret = midsema_node_type(MIDPAR_GET_NODE(class_), res);

    return name_idx + 1;
}

// some operator overloads are ambiguous until the parameters have been parsed
static void disambig_operator_overload(struct midpar_FuncDecl *self)
{
    assert(self->is_op_overload);

    bool implicit_this =
        midpar_func_parent(self)->type == MIDSEMA_SCOPETYPE_CLASS;
    mid_isize n_params = self->params.len + implicit_this;
    if (n_params == 1) {
        switch (self->op_overload) {
        case MIDPAR_EXPRTYPE_ADD:
            self->op_overload = MIDPAR_EXPRTYPE_UNARY_PLUS;
            break;

        case MIDPAR_EXPRTYPE_SUB:
            self->op_overload = MIDPAR_EXPRTYPE_UNARY_MINUS;
            break;

        case MIDPAR_EXPRTYPE_MUL:
            self->op_overload = MIDPAR_EXPRTYPE_DEREF;
            break;

        case MIDPAR_EXPRTYPE_BITWISE_AND:
            self->op_overload = MIDPAR_EXPRTYPE_REF;
            break;

        case MIDPAR_EXPRTYPE_POSTFIX_INC:
            self->op_overload = MIDPAR_EXPRTYPE_PREFIX_INC;
            break;

        case MIDPAR_EXPRTYPE_POSTFIX_DEC:
            self->op_overload = MIDPAR_EXPRTYPE_PREFIX_DEC;
            break;

        default:
            break;
        }
    }
}

static void add_func_to_scope(struct midsema_Scope *scope,
                              struct midpar_FuncDecl *self)
{
    bool is_tmplt = midpar_node_is_templated(MIDPAR_GET_NODE(self));
    enum midsema_IdentType type =
        is_tmplt ? MIDSEMA_IDENTTYPE_TMPLT_FUNC : MIDSEMA_IDENTTYPE_FUNC;

    const struct midsema_Ident *old = midsema_add_ident(
        scope, &(struct midsema_Ident){.name = self->name,
                                       .decl = MIDPAR_GET_NODE(self),
                                       .type = type});

    if (old)
        self->ident_idx = old - scope->idents.arr;
    else
        self->ident_idx = scope->idents.len - 1;
}

// default_args can't be const cuz of sum discarding qualifiers in nested ptrs
// stuff idk.
// returns true if there's a missing default argument
// out_bad_idx        - idx of the bad argument if there is one
static bool missing_default_args(struct midpar_Expr **default_args, mid_isize n,
                                 mid_isize *out_bad_idx)
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

static void register_default_args(struct midpar_FuncDecl *decl,
                                  struct mid_DiagVec *diags)
{
    auto default_args = &midpar_func_ident(decl)->func_info.default_args;
    if (!*default_args)
        *default_args = mid_calloc(decl->params.len, sizeof(**default_args));

    for (mid_isize i = 0; i < decl->params.len; ++i) {
        auto default_arg = (*default_args)[i];

        auto node = (struct midpar_ASTNode *)decl->params.arr[i];
        auto param = node->var_decl.insts.arr[0];
        if (!param->init.expr) // not a default arg
            continue;

        if (default_arg) {
            midgen_dynpush(diags, middiag_ident_redefined_err(
                                      param->name, node->start,
                                      MIDDIAG_ERR_BAD_DEFAULT_ARGUMENT));
            continue;
        }

        (*default_args)[i] = param->init.expr;
    }

    mid_isize bad;
    if (missing_default_args(*default_args, decl->params.len, &bad))
        midgen_dynpush(diags,
                       missing_default_arg_err(
                           decl->name, MIDPAR_GET_START(decl->params.arr[bad]),
                           MIDDIAG_ERR_BAD_DEFAULT_ARGUMENT));
}

static midlex_TokenIter skip_til_body_end(midlex_TokenIter tok)
{
    if (tok->type == MIDLEX_TOKENTYPE_COLON)
        tok = parse_ctor_init_list(nullptr, nullptr, tok, nullptr, nullptr);

    if (tok->type == MIDLEX_TOKENTYPE_ASSIGN)
        return tok + 2;
    else if (tok->type == MIDLEX_TOKENTYPE_L_CURLY)
        return midpar_find_twin_curly(tok, nullptr) + 1;
    else
        return tok;
}

static bool valid_body_start(const struct midlex_Token *tok)
{
    return tok->type == MIDLEX_TOKENTYPE_L_CURLY ||
           tok->type == MIDLEX_TOKENTYPE_COLON;
}

midlex_TokenIter midpar_parse_func_decl(struct midpar_FuncDecl *self,
                                        midlex_TokenIter start,
                                        struct midsema_Scope *parent_scope,
                                        bool skip_def,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags)
{
    *self = (struct midpar_FuncDecl){.ident_idx = -1};

    struct midsema_Scope *res;
    midlex_TokenIter type_end =
        parse_func_type(self, start, parent_scope, &res, allocs, diags);
    if (type_end->type != MIDLEX_TOKENTYPE_L_PAREN)
        MID_CRASH("function missing left paren");

    self->param_scope = create_scope(res, MIDPAR_GET_NODE(self), allocs,
                                     MIDSEMA_SCOPETYPE_FUNC_PARAMS);

    auto lparen = type_end;
    midlex_TokenIter rparen;
    self->params = midpar_parse_func_params(
        lparen, &rparen, MIDPAR_GET_NODE(self), self->param_scope, true,
        &self->variadic, allocs, diags);
    midlex_TokenIter body_start = parse_func_quals(
        rparen + 1, &self->quals, self->ret.squals.is_constexpr, diags);
    if (self->is_op_overload)
        disambig_operator_overload(self);
    if (self->name)
        add_func_to_scope(res, self);
    register_default_args(self, diags);

    if (!skip_def)
        midsema_typecheck_func_decl(self, diags);

    if (!valid_body_start(body_start))
        return body_start;
    self->def_start = body_start;
    self->has_def = true;

    if (skip_def) {
        return skip_til_body_end(body_start);
    } else {
        midlex_TokenIter body_end =
            midpar_parse_func_body(self, body_start, allocs, diags);
        return body_end + 1;
    }
}

midlex_TokenIter
midpar_parse_tor(struct midpar_FuncDecl *self, midlex_TokenIter start,
                 struct midsema_Scope *parent_scope, bool skip_def,
                 struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    *self = (struct midpar_FuncDecl){.ident_idx = -1, .is_tor = true};

    struct midpar_Class *class_;
    midlex_TokenIter lparen =
        parse_tor_type(self, start, &class_, parent_scope, diags);

    self->name = self->is_dtor ? midpar_dtor_name : midpar_ctor_name;

    auto c_scope = midsema_deref_identptr(&class_->ident)->class_info.def_scope;
    self->param_scope = create_scope(c_scope, MIDPAR_GET_NODE(self), allocs,
                                     MIDSEMA_SCOPETYPE_FUNC_PARAMS);

    midlex_TokenIter rparen;
    self->params = midpar_parse_func_params(
        lparen, &rparen, MIDPAR_GET_NODE(self), self->param_scope, true,
        &self->variadic, allocs, diags);
    midlex_TokenIter body_start = parse_func_quals(
        rparen + 1, &self->quals, self->quals.is_constexpr, diags);
    add_func_to_scope(c_scope, self);
    register_default_args(self, diags);

    if (!skip_def)
        midsema_typecheck_func_decl(self, diags);

    if (!valid_body_start(body_start))
        return body_start;
    self->def_start = body_start;
    self->has_def = true;

    if (skip_def) {
        return skip_til_body_end(body_start);
    } else {
        midlex_TokenIter body_end =
            midpar_parse_func_body(self, body_start, allocs, diags);
        return body_end + 1;
    }
}
