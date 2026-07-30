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

static struct Diag missing_default_arg_err(const char *func,
                                           const struct Lexer_Token *tok,
                                           enum ErrorType err_type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("function '%s' missing default argument", func),
        .err = err_type,
        .type = DIAGTYPE_ERROR,
    };
}

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self)
{
    gen_dyndeinit(&self->nodes);
    gen_dyndeinit(&self->params);
    Parser_Type_deinit(&self->ret);
}

void Parser_copy_func_decl(struct Parser_FuncDecl *dest,
                           const struct Parser_FuncDecl *src,
                           struct Sema_Scope *dest_scope,
                           struct Parser_Allocators *allocs)
{
    *dest = *src;

    auto old_ident =
        Sema_add_ident_copy(dest_scope, Parser_func_ident(src), false, allocs);
    if (old_ident)
        dest->ident_idx = old_ident - dest_scope->idents.arr;
    else
        dest->ident_idx = dest_scope->idents.len - 1;

    dest->ret = Parser_copy_type(&src->ret);

    gen_bumpmalloc(&allocs->scope, &dest->param_scope);
    *dest->param_scope = Sema_create_empty_scope(
        src->param_scope->type, dest_scope, PARSER_GET_NODE(dest));

    auto params_nodes =
        Parser_copy_nodepvec((struct Parser_ASTNodePVec *)&src->params,
                             PARSER_GET_NODE(dest), dest->param_scope, allocs);
    dest->params = (struct Parser_VarDeclPVec){.arr = (void *)params_nodes.arr,
                                               .len = params_nodes.len,
                                               .cap = params_nodes.cap};

    if (src->nodes.len > 0) {
        struct Sema_Scope *def_scope;
        gen_bumpmalloc(&allocs->scope, &def_scope);
        *def_scope = (struct Sema_Scope){.parent = dest_scope,
                                         .node = PARSER_GET_NODE(dest),
                                         .type = SEMA_SCOPETYPE_FUNC};
        Parser_func_ident(dest)->func_info.def_scope = def_scope;
        dest->nodes = Parser_copy_nodepvec(&src->nodes, PARSER_GET_NODE(dest),
                                           def_scope, allocs);
    }
}

struct Sema_Scope *Parser_func_parent(const struct Parser_FuncDecl *func)
{
    auto ret = func->param_scope->parent;
    assert(ret);
    return ret;
}

struct Sema_Ident *Parser_func_ident(const struct Parser_FuncDecl *func)
{
    assert(func->ident_idx != -1);
    auto parent = Parser_func_parent(func);
    return &parent->idents.arr[func->ident_idx];
}

// accounts for functions taking a singular void parameter
// example: int f(void)
static void account_for_void_param(struct Parser_VarDeclPVec *params)
{
    if (params->len == 1 &&
        Parser_type_is_void(&params->arr[0]->insts.arr[0].type)) {
        gen_dyndeinit(params);
    }
}

struct Parser_VarDeclPVec Parser_parse_func_params(
    const struct Lexer_Token *toks, isize_t lparen, isize_t *out_rparen,
    struct Parser_ASTNode *parent, struct Sema_Scope *scope, bool add_to_scope,
    bool *out_variadic, struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    struct Parser_VarDeclPVec params = {};

    isize_t rparen = Parser_find_twin_paren(toks, lparen, ISIZE_MAX);
    if (out_rparen)
        *out_rparen = rparen;

    if (rparen == -1) {
        gen_dynpush(diags, Diag_expected_token_err("')'", &toks[lparen],
                                                   ERRORTYPE_MISSING_PAREN));
        return params;
    }

    if (out_variadic)
        *out_variadic = false;

    for (isize_t i = lparen + 1; i < rparen; ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_ELLIPSIS) {
            if (out_variadic)
                *out_variadic = true;
            if (i + 1 < rparen)
                gen_dynpush(diags,
                            Diag_expected_token_err("')'", &toks[lparen],
                                                    ERRORTYPE_MISSING_PAREN));
            break;
        } else {
            struct Parser_ASTNode *child;
            gen_bumpmalloc(&allocs->ast, &child);
            *child =
                (struct Parser_ASTNode){.parent = parent,
                                        .start = &toks[i],
                                        .type = PARSER_ASTNODETYPE_VAR_DECL};
            i = Parser_parse_var_decl(
                &child->var_decl, toks, i, PARSER_PARAM_ENDTYPES,
                (struct Parser_ParseVarDeclFlags){.add_to_scope = add_to_scope,
                                                  .single_inst = true},
                scope, allocs, diags);
            gen_dynpush(&params, (void *)child);
        }
    }

    account_for_void_param(&params);

    return params;
}

static bool is_func_quals_end(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_SEMICOLON ||
           type == LEXER_TOKENTYPE_L_CURLY || type == LEXER_TOKENTYPE_ASSIGN;
}

static void set_quals_flag(const struct Lexer_Token *tok,
                           struct Parser_FuncQuals *quals,
                           struct DiagVec *diags)
{
    switch (tok->type) {
    case LEXER_TOKENTYPE_CONST:
        quals->is_const = true;
        break;

    case LEXER_TOKENTYPE_VOLATILE:
        quals->is_volatile = true;
        break;

    case LEXER_TOKENTYPE_BITWISE_AND:
        quals->lv_ref = true;
        break;

    case LEXER_TOKENTYPE_LOGICAL_AND:
        quals->rv_ref = true;
        break;

    case LEXER_TOKENTYPE_FINAL:
        quals->is_final = true;
        break;

    case LEXER_TOKENTYPE_OVERRIDE:
        quals->is_override = true;
        break;

    default:
        gen_dynpush(diags, Diag_expected_token_err("qualifier", tok,
                                                   ERRORTYPE_UNEXPECTED_TOKEN));
        break;
    }
}

isize_t Parser_parse_func_quals(const struct Lexer_Token *toks, isize_t start,
                                struct Parser_FuncQuals *quals,
                                struct DiagVec *diags)
{
    *quals = (struct Parser_FuncQuals){};

    isize_t i;
    for (i = start; !is_func_quals_end(toks[i].type); ++i) {
        set_quals_flag(&toks[i], quals, diags);
    }

    if (toks[i].type != LEXER_TOKENTYPE_ASSIGN)
        return i;

    ++i;
    if (toks[i].type == LEXER_TOKENTYPE_DELETE)
        quals->is_delete = true;
    else if (toks[i].type == LEXER_TOKENTYPE_DEFAULT)
        quals->is_default = true;
    else
        gen_dynpush(diags, Diag_expected_token_err("qualifier", &toks[i],
                                                   ERRORTYPE_UNEXPECTED_TOKEN));
    return i + 1;
}

static struct Sema_Scope *create_scope(struct Sema_Scope *scope,
                                       struct Parser_ASTNode *node,
                                       struct Parser_Allocators *allocs,
                                       enum Sema_ScopeType type)
{
    struct Sema_Scope *child;
    gen_bumpmalloc(&allocs->scope, &child);
    *child = (struct Sema_Scope){.parent = scope, .node = node, .type = type};
    gen_dynpush(&scope->childs, child);

    return child;
}

static void add_func_def(struct Parser_FuncDecl *func, struct DiagVec *diags)
{
    if (!func->name)
        return;

    auto ident = Parser_func_ident(func);
    if (ident->def)
        gen_dynpush(diags, Diag_ident_redefined_err(func->name, func->def_start,
                                                    ERRORTYPE_BAD_IDENTIFIER));
    ident->def = PARSER_GET_NODE(func);
}

static void copy_params_to_scope(struct Parser_FuncDecl *src,
                                 struct Sema_Scope *dest)
{
    for (isize_t i = 0; i < src->param_scope->idents.len; ++i) {
        auto ident = &src->param_scope->idents.arr[i];
        assert(ident->type == SEMA_IDENTTYPE_VAR);

        gen_dynpush(&dest->idents, Sema_copy_var_ident(ident));
    }
}

static struct Sema_Scope *setup_def_scope(struct Parser_FuncDecl *self,
                                          struct Parser_Allocators *allocs,
                                          struct DiagVec *diags)
{
    auto def = &Parser_func_ident(self)->func_info.def_scope;
    if (*def) {
        gen_dynpush(diags,
                    Diag_ident_redefined_err(self->name, PARSER_GET_START(self),
                                             ERRORTYPE_BAD_IDENTIFIER));
    }

    *def = create_scope(Parser_func_parent(self), PARSER_GET_NODE(self), allocs,
                        SEMA_SCOPETYPE_FUNC);
    // necessary to make the function parameters visible in the func body
    copy_params_to_scope(self, *def);

    return *def;
}

isize_t Parser_parse_func_body(struct Parser_FuncDecl *self,
                               const struct Lexer_Token *toks, isize_t lcurly,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    add_func_def(self, diags);

    isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
    if (rcurly == -1) {
        gen_dynpush(diags, Diag_expected_token_err("'}'", &toks[lcurly],
                                                   ERRORTYPE_MISSING_CURLY));
        return lcurly + 1;
    }

    auto def = setup_def_scope(self, allocs, diags);

    for (isize_t i = lcurly + 1; i < rcurly;) {
        auto child = Parser_parse_node(
            toks, i, &i, PARSER_GET_NODE(self), def,
            (struct Parser_ParseNodeFlags){.skip_def = false}, allocs, diags);
        gen_dynpush(&self->nodes, child);
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
static enum Parser_ExprType
parse_operator_overload(const struct Lexer_Token *toks, isize_t op,
                        isize_t *out_end, struct DiagVec *diags)
{
    if (out_end)
        *out_end = op + 1;

    switch (toks[op].type) {
    case LEXER_TOKENTYPE_ADD:
        return PARSER_EXPRTYPE_ADD;

    case LEXER_TOKENTYPE_SUB:
        return PARSER_EXPRTYPE_SUB;

    case LEXER_TOKENTYPE_MUL:
        return PARSER_EXPRTYPE_MUL;

    case LEXER_TOKENTYPE_DIV:
        return PARSER_EXPRTYPE_DIV;

    case LEXER_TOKENTYPE_MOD:
        return PARSER_EXPRTYPE_MOD;

    case LEXER_TOKENTYPE_INC:
        return PARSER_EXPRTYPE_POSTFIX_INC;

    case LEXER_TOKENTYPE_DEC:
        return PARSER_EXPRTYPE_POSTFIX_DEC;

    case LEXER_TOKENTYPE_EQ:
        return PARSER_EXPRTYPE_EQ;

    case LEXER_TOKENTYPE_NEQ:
        return PARSER_EXPRTYPE_NEQ;

    case LEXER_TOKENTYPE_GT:
        return PARSER_EXPRTYPE_GT;

    case LEXER_TOKENTYPE_LT:
        return PARSER_EXPRTYPE_LT;

    case LEXER_TOKENTYPE_GTEQ:
        return PARSER_EXPRTYPE_GTEQ;

    case LEXER_TOKENTYPE_LTEQ:
        return PARSER_EXPRTYPE_LTEQ;

    case LEXER_TOKENTYPE_LOGICAL_NOT:
        return PARSER_EXPRTYPE_LOGICAL_NOT;

    case LEXER_TOKENTYPE_LOGICAL_AND:
        return PARSER_EXPRTYPE_LOGICAL_AND;

    case LEXER_TOKENTYPE_LOGICAL_OR:
        return PARSER_EXPRTYPE_LOGICAL_OR;

    case LEXER_TOKENTYPE_BITWISE_NOT:
        return PARSER_EXPRTYPE_BITWISE_NOT;

    case LEXER_TOKENTYPE_BITWISE_AND:
        return PARSER_EXPRTYPE_BITWISE_AND;

    case LEXER_TOKENTYPE_BITWISE_OR:
        return PARSER_EXPRTYPE_BITWISE_OR;

    case LEXER_TOKENTYPE_BITWISE_XOR:
        return PARSER_EXPRTYPE_BITWISE_XOR;

    case LEXER_TOKENTYPE_LEFT_SHIFT:
        return PARSER_EXPRTYPE_LEFT_SHIFT;

    case LEXER_TOKENTYPE_RIGHT_SHIFT:
        return PARSER_EXPRTYPE_RIGHT_SHIFT;

    case LEXER_TOKENTYPE_ASSIGN:
        return PARSER_EXPRTYPE_ASSIGN;

    case LEXER_TOKENTYPE_ADD_ASSIGN:
        return PARSER_EXPRTYPE_ADD_ASSIGN;

    case LEXER_TOKENTYPE_SUB_ASSIGN:
        return PARSER_EXPRTYPE_SUB_ASSIGN;

    case LEXER_TOKENTYPE_MUL_ASSIGN:
        return PARSER_EXPRTYPE_MUL_ASSIGN;

    case LEXER_TOKENTYPE_DIV_ASSIGN:
        return PARSER_EXPRTYPE_DIV_ASSIGN;

    case LEXER_TOKENTYPE_MOD_ASSIGN:
        return PARSER_EXPRTYPE_MOD_ASSIGN;

    case LEXER_TOKENTYPE_AND_ASSIGN:
        return PARSER_EXPRTYPE_AND_ASSIGN;

    case LEXER_TOKENTYPE_OR_ASSIGN:
        return PARSER_EXPRTYPE_OR_ASSIGN;

    case LEXER_TOKENTYPE_XOR_ASSIGN:
        return PARSER_EXPRTYPE_XOR_ASSIGN;

    case LEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        return PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;

    case LEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        return PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;

    case LEXER_TOKENTYPE_L_SQBRACKET:
        if (toks[op + 1].type != LEXER_TOKENTYPE_R_SQBRACKET)
            gen_dynpush(diags, Diag_expected_token_err(
                                   "]", &toks[op], ERRORTYPE_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return PARSER_EXPRTYPE_ARRAY_SUBSCR;

    case LEXER_TOKENTYPE_PTR_MEMB_SEL:
        return PARSER_EXPRTYPE_PTR_MEMB_SEL;

    case LEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        return PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;

    case LEXER_TOKENTYPE_L_PAREN:
        if (toks[op + 1].type != LEXER_TOKENTYPE_R_PAREN)
            gen_dynpush(diags, Diag_expected_token_err(
                                   ")", &toks[op], ERRORTYPE_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return PARSER_EXPRTYPE_FUNC_CALL;

    case LEXER_TOKENTYPE_COMMA:
        return PARSER_EXPRTYPE_COMMA;

    case LEXER_TOKENTYPE_NEW:
        if (toks[op + 1].type == LEXER_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (toks[op + 2].type != LEXER_TOKENTYPE_R_SQBRACKET)
                gen_dynpush(diags,
                            Diag_expected_token_err("]", &toks[op + 1],
                                                    ERRORTYPE_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return PARSER_EXPRTYPE_NEW_ARR;
        } else {
            return PARSER_EXPRTYPE_NEW;
        }

    case LEXER_TOKENTYPE_DELETE:
        if (toks[op + 1].type == LEXER_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (toks[op + 2].type != LEXER_TOKENTYPE_R_SQBRACKET)
                gen_dynpush(diags,
                            Diag_expected_token_err("]", &toks[op + 1],
                                                    ERRORTYPE_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return PARSER_EXPRTYPE_DELETE_ARR;
        } else {
            return PARSER_EXPRTYPE_DELETE;
        }

    default:
        gen_dynpush(diags, ((struct Diag){
                               .pos = toks[op].pos,
                               .line = toks[op].line,
                               .msg = strdup("can't overload operator"),
                               .err = ERRORTYPE_BAD_OP_OVERLOAD,
                               .type = DIAGTYPE_ERROR,
                           }));
        if (out_end)
            --*out_end;
        // just default to add for now
        return PARSER_EXPRTYPE_ADD;
    }
}

static isize_t parse_func_type(struct Parser_FuncDecl *self,
                               const struct Lexer_Token *toks, isize_t start,
                               struct Sema_Scope *parent_scope,
                               struct Sema_Scope **out_res,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    isize_t type_end;
    isize_t name;
    self->ret = Parser_parse_type(toks, start, &type_end, parent_scope, &name,
                                  false, allocs, diags);

    auto res = name == -1 ? parent_scope
                          : Parser_parse_scope_res(toks, name, &name,
                                                   parent_scope, diags);
    if (out_res)
        *out_res = res;

    self->name = name == -1 || toks[name].type != LEXER_TOKENTYPE_IDENTIFIER
                     ? NULL
                     : toks[name].ident;

    if (!self->name) {
        gen_dynpush(diags, Diag_expected_token_err("identifier", &toks[start],
                                                   ERRORTYPE_MISSING_TOKEN));
        self->name = "INVALID-FUNC-NAME";
    } else if (!strcmp(self->name, "operator")) {
        self->is_op_overload = true;
        self->op_overload =
            parse_operator_overload(toks, type_end, &type_end, diags);
    }

    return type_end;
}

static struct Parser_Class *find_tor_class(struct Sema_Scope *scope,
                                           const char *name)
{
    if (scope->type == SEMA_SCOPETYPE_CLASS) {
        if (!strcmp(scope->node->class_.name, name))
            return &scope->node->class_;
    }

    for (isize_t i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];

        if (ident->type != SEMA_IDENTTYPE_CLASS)
            continue;

        if (!strcmp(ident->name, name))
            return &ident->decl->class_;
    }

    return NULL;
}

static isize_t parse_tor_type(struct Parser_FuncDecl *self,
                              const struct Lexer_Token *toks, isize_t start,
                              struct Parser_Class **out_class,
                              struct Sema_Scope *parent_scope,
                              struct DiagVec *diags)
{
    isize_t name_idx;
    auto res =
        Parser_parse_scope_res(toks, start, &name_idx, parent_scope, diags);

    self->is_dtor = toks[name_idx].type == LEXER_TOKENTYPE_BITWISE_NOT;
    if (self->is_dtor)
        ++name_idx;

    assert(toks[name_idx].type == LEXER_TOKENTYPE_IDENTIFIER);
    auto class_ = find_tor_class(res, toks[name_idx].ident);
    assert(class_);
    if (out_class)
        *out_class = class_;
    self->ret = Sema_node_type(PARSER_GET_NODE(class_), res, NULL);

    return name_idx + 1;
}

// some operator overloads are ambiguous until the parameters have been parsed
static void disambig_operator_overload(struct Parser_FuncDecl *self)
{
    assert(self->is_op_overload);

    bool implicit_this = Parser_func_parent(self)->type == SEMA_SCOPETYPE_CLASS;
    isize_t n_params = self->params.len + implicit_this;
    if (n_params == 1) {
        switch (self->op_overload) {
        case PARSER_EXPRTYPE_ADD:
            self->op_overload = PARSER_EXPRTYPE_UNARY_PLUS;
            break;

        case PARSER_EXPRTYPE_SUB:
            self->op_overload = PARSER_EXPRTYPE_UNARY_MINUS;
            break;

        case PARSER_EXPRTYPE_MUL:
            self->op_overload = PARSER_EXPRTYPE_DEREF;
            break;

        case PARSER_EXPRTYPE_BITWISE_AND:
            self->op_overload = PARSER_EXPRTYPE_REF;
            break;

        case PARSER_EXPRTYPE_POSTFIX_INC:
            self->op_overload = PARSER_EXPRTYPE_PREFIX_INC;
            break;

        case PARSER_EXPRTYPE_POSTFIX_DEC:
            self->op_overload = PARSER_EXPRTYPE_PREFIX_DEC;
            break;

        default:
            break;
        }
    }
}

static void add_func_to_scope(struct Sema_Scope *scope,
                              struct Parser_FuncDecl *self)
{
    bool is_tmplt = Parser_node_is_templated(PARSER_GET_NODE(self));
    enum Sema_IdentType type =
        is_tmplt ? SEMA_IDENTTYPE_TMPLT_FUNC : SEMA_IDENTTYPE_FUNC;

    const struct Sema_Ident *old = Sema_add_ident(
        scope, &(struct Sema_Ident){.name = self->name,
                                    .decl = PARSER_GET_NODE(self),
                                    .type = type});

    if (old)
        self->ident_idx = old - scope->idents.arr;
    else
        self->ident_idx = scope->idents.len - 1;
}

// default_args can't be const cuz of sum discarding qualifiers in nested ptrs
// stuff idk
static bool missing_default_args(struct Parser_Expr **default_args, isize_t n,
                                 isize_t *out_bad_idx)
{
    bool found_default = false;
    for (isize_t i = 0; i < n; ++i) {
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

static void register_default_args(struct Parser_FuncDecl *decl,
                                  struct DiagVec *diags)
{
    auto default_args = &Parser_func_ident(decl)->func_info.default_args;
    if (!*default_args)
        *default_args = mid_calloc(decl->params.len, sizeof(**default_args));

    for (isize_t i = 0; i < decl->params.len; ++i) {
        auto default_arg = (*default_args)[i];

        auto node = (struct Parser_ASTNode *)decl->params.arr[i];
        auto param = &node->var_decl.insts.arr[0];
        if (!param->init.expr) // not a default arg
            continue;

        if (default_arg) {
            gen_dynpush(diags, Diag_ident_redefined_err(
                                   param->name, node->start,
                                   ERRORTYPE_BAD_DEFAULT_ARGUMENT));
            continue;
        }

        (*default_args)[i] = param->init.expr;
    }

    isize_t bad;
    if (missing_default_args(*default_args, decl->params.len, &bad))
        gen_dynpush(diags,
                    missing_default_arg_err(
                        decl->name, PARSER_GET_START(decl->params.arr[bad]),
                        ERRORTYPE_BAD_DEFAULT_ARGUMENT));
}

isize_t Parser_parse_func_decl(struct Parser_FuncDecl *self,
                               const struct Lexer_Token *toks, isize_t start,
                               struct Sema_Scope *parent_scope, bool skip_def,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    *self = (struct Parser_FuncDecl){.ident_idx = -1};

    struct Sema_Scope *res;
    isize_t type_end =
        parse_func_type(self, toks, start, parent_scope, &res, allocs, diags);
    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN)
        CRASH("function missing left paren");

    self->param_scope = create_scope(res, PARSER_GET_NODE(self), allocs,
                                     SEMA_SCOPETYPE_FUNC_PARAMS);

    isize_t lparen = type_end;
    isize_t rparen;
    self->params = Parser_parse_func_params(
        toks, lparen, &rparen, PARSER_GET_NODE(self), self->param_scope, true,
        &self->variadic, allocs, diags);
    isize_t lcurly =
        Parser_parse_func_quals(toks, rparen + 1, &self->quals, diags);
    if (self->is_op_overload)
        disambig_operator_overload(self);
    if (self->name)
        add_func_to_scope(res, self);
    register_default_args(self, diags);

    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY)
        return lcurly;
    self->def_start = &toks[lcurly];
    self->has_def = true;

    if (skip_def) {
        isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
        return rcurly == -1 ? lcurly + 1 : rcurly + 1;
    } else {
        isize_t rcurly =
            Parser_parse_func_body(self, toks, lcurly, allocs, diags);
        return rcurly + 1;
    }
}

isize_t Parser_parse_tor(struct Parser_FuncDecl *self,
                         const struct Lexer_Token *toks, isize_t start,
                         struct Sema_Scope *parent_scope, bool skip_def,
                         struct Parser_Allocators *allocs,
                         struct DiagVec *diags)
{
    *self = (struct Parser_FuncDecl){.ident_idx = -1, .is_tor = true};

    struct Parser_Class *class_;
    isize_t lparen =
        parse_tor_type(self, toks, start, &class_, parent_scope, diags);

    self->name = self->is_dtor ? Parser_dtor_name : Parser_ctor_name;

    auto c_scope = Sema_deref_identptr(&class_->ident)->class_info.def_scope;
    self->param_scope = create_scope(c_scope, PARSER_GET_NODE(self), allocs,
                                     SEMA_SCOPETYPE_FUNC_PARAMS);

    isize_t rparen;
    self->params = Parser_parse_func_params(
        toks, lparen, &rparen, PARSER_GET_NODE(self), self->param_scope, true,
        &self->variadic, allocs, diags);
    isize_t lcurly =
        Parser_parse_func_quals(toks, rparen + 1, &self->quals, diags);
    add_func_to_scope(c_scope, self);
    register_default_args(self, diags);

    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY)
        return lcurly;
    self->def_start = &toks[lcurly];
    self->has_def = true;

    if (skip_def) {
        isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
        return rcurly == -1 ? lcurly + 1 : rcurly + 1;
    } else {
        isize_t rcurly =
            Parser_parse_func_body(self, toks, lcurly, allocs, diags);
        return rcurly + 1;
    }
}

bool Parser_func_is_method(const struct Parser_FuncDecl *self)
{
    return Parser_func_parent(self)->type == SEMA_SCOPETYPE_CLASS;
}

bool Parser_func_is_ctor(const struct Parser_FuncDecl *self)
{
    return self->is_tor && !self->is_dtor;
}

bool Parser_func_takes_implicit_this(const struct Parser_FuncDecl *self,
                                     bool cnt_ctors)
{
    if (cnt_ctors && Parser_func_is_ctor(self))
        return true;
    return Parser_func_is_method(self) && !Parser_func_is_ctor(self) &&
           !self->ret.squals.is_static;
}

struct Parser_Type Parser_implicit_this_type(const struct Parser_FuncDecl *self)
{
    const struct Sema_Scope *parent = Parser_func_parent(self);
    return Sema_node_type(parent->node, parent->parent, NULL);
}

bool Parser_func_is_main(const struct Parser_FuncDecl *self)
{
    return self->param_scope->parent->type == SEMA_SCOPETYPE_ROOT &&
           !strcmp(self->name, "main");
}
