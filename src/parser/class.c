#include "class.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/end_types.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <string.h>

void midpar_Class_deinit(struct midpar_Class *self)
{
    midgen_dyndeinit(&self->childs);
    midgen_dyndeinit(&self->pub_childs);
    midgen_dyndeinit(&self->priv_childs);
    midgen_dyndeinit(&self->prot_childs);
}

// takes an array of ptrs to nodes in old_nodes and transforms each ptr to the
// one at the same idx in new_nodes
static struct midpar_ASTNodePVec
transf_node_ptrs(const struct midpar_ASTNodePVec *ptrs,
                 struct midpar_ASTNode *const *old_nodes,
                 struct midpar_ASTNode *const *new_nodes, mid_isize n_nodes)
{
    struct midpar_ASTNodePVec ret = {};
    midgen_dynreserve(&ret, ptrs->len);

    for (mid_isize p_i = 0; p_i < ptrs->len; ++p_i) {
        mid_isize n_i;
        for (n_i = 0; n_i < n_nodes; ++n_i) {
            if (old_nodes[n_i] == ptrs->arr[p_i])
                break;
        }

        if (n_i == n_nodes)
            MID_CRASH("ptr not in old_nodes");

        midgen_dynpush(&ret, new_nodes[n_i]);
    }

    return ret;
}

void midpar_copy_class(struct midpar_Class *dest,
                       const struct midpar_Class *src,
                       struct midsema_Scope *dest_scope,
                       struct midpar_Allocators *allocs)
{
    *dest = *src;

    auto old_ident = midsema_add_ident_copy(
        dest_scope, midsema_deref_identptr(&src->ident), false, allocs);
    if (old_ident)
        dest->ident = midsema_create_identptr(old_ident);
    else
        dest->ident = midsema_identptr_to_last(dest_scope);

    if (dest->childs.len > 0) {
        struct midsema_Scope *child_scope;
        midgen_bumpmalloc(&allocs->scope, &child_scope);
        *child_scope =
            (struct midsema_Scope){.parent = midpar_class_parent(dest),
                                   .node = MIDPAR_GET_NODE(dest),
                                   .type = MIDSEMA_SCOPETYPE_CLASS};
        midsema_deref_identptr(&dest->ident)->class_info.def_scope =
            child_scope;

        dest->childs = midpar_copy_nodepvec(&src->childs, MIDPAR_GET_NODE(dest),
                                            child_scope, allocs);
        dest->pub_childs = transf_node_ptrs(&src->pub_childs, src->childs.arr,
                                            dest->childs.arr, src->childs.len);
        dest->priv_childs = transf_node_ptrs(&src->priv_childs, src->childs.arr,
                                             dest->childs.arr, src->childs.len);
        dest->prot_childs = transf_node_ptrs(&src->prot_childs, src->childs.arr,
                                             dest->childs.arr, src->childs.len);
    }

    if (src->var) {
        midgen_bumpmalloc(&allocs->ast, (void **)&dest->var);
        midpar_copy_node(MIDPAR_GET_NODE(dest->var), MIDPAR_GET_NODE(src->var),
                         MIDPAR_GET_NODE(dest), midpar_class_parent(dest),
                         allocs);
    }
}

struct midsema_Scope *midpar_class_parent(const struct midpar_Class *self)
{
    return self->ident.parent;
}

// parses the inheritance part of a class
// class SuperHuman : Human { ... };
//                  ^       ^
//                colon   return
static mid_isize parse_class_inheritance(struct midpar_Class *self,
                                         const struct midlex_Token *toks,
                                         mid_isize colon,
                                         struct mid_DiagVec *diags)
{
    mid_isize ident = colon + 1;
    if (toks[ident].type != MIDLEX_TOKENTYPE_IDENTIFIER) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("identifier", &toks[colon],
                                                  MIDDIAG_ERR_MISSING_TOKEN));
        return ident;
    }

    const char *super_name = toks[ident].ident;
    struct midpar_Class *super =
        &midsema_find_ident_const(midpar_class_parent(self), super_name)
             ->def->class_;

    if (!super)
        midgen_dynpush(diags, ((struct mid_Diag){
                                  .pos = toks[ident].pos,
                                  .line = toks[ident].line,
                                  .msg = midprt_fmt_to_str("'%s' is undefined",
                                                           super_name),
                                  .err = MIDDIAG_ERR_BAD_SUPERCLASS,
                                  .type = MIDDIAG_TYPE_ERROR,
                              }));
    else if (MIDPAR_GET_TYPE(super) != MIDPAR_ASTNODETYPE_CLASS)
        midgen_dynpush(
            diags, ((struct mid_Diag){
                       .pos = toks[ident].pos,
                       .line = toks[ident].line,
                       .msg = midprt_fmt_to_str("'%s' is not a defined class",
                                                super_name),
                       .err = MIDDIAG_ERR_BAD_SUPERCLASS,
                       .type = MIDDIAG_TYPE_ERROR,
                   }));
    else
        midgen_dynpush(&self->supers, super);

    return ident + 1;
}

static enum midpar_ClassType parse_class_type(const struct midlex_Token *toks,
                                              mid_isize start)
{
    if (toks[start].type == MIDLEX_TOKENTYPE_UNION) {
        return MIDPAR_CLASSTYPE_UNION;
    } else if (toks[start].type == MIDLEX_TOKENTYPE_STRUCT) {
        return MIDPAR_CLASSTYPE_STRUCT;
    } else if (toks[start].type == MIDLEX_TOKENTYPE_CLASS) {
        return MIDPAR_CLASSTYPE_CLASS;
    } else {
        MID_CRASH("tried to parse something that isn't a class");
    }
}

static mid_isize parse_class_entry(struct midpar_Class *self,
                                   const struct midlex_Token *toks,
                                   mid_isize start,
                                   struct midsema_Scope *parent_scope,
                                   struct mid_DiagVec *diags)
{
    self->type = parse_class_type(toks, start);

    mid_isize ident = start + 1;
    self->ident.parent =
        midpar_parse_scope_res(toks, ident, &ident, parent_scope, diags);
    if (toks[ident].type != MIDLEX_TOKENTYPE_IDENTIFIER) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("identifier", &toks[start],
                                                  MIDDIAG_ERR_MISSING_TOKEN));
        --ident;
        self->name = "INVALID-NAME";
    } else {
        self->name = toks[ident].ident;
    }

    mid_isize end = ident + 1;
    if (toks[end].type == MIDLEX_TOKENTYPE_COLON)
        end = parse_class_inheritance(self, toks, end, diags);

    return end;
}

static void parse_node_def(struct midpar_ASTNode *node,
                           struct midsema_Scope *scope,
                           const struct midlex_Token *toks,
                           struct midpar_Allocators *allocs,
                           struct mid_DiagVec *diags)
{
    if (node->type == MIDPAR_ASTNODETYPE_VAR_DECL) {
        midpar_parse_var_decl_def(toks, MIDPAR_VARDECL_ENDTYPES,
                                  &node->var_decl, false, scope, allocs, diags);
    } else if (node->type == MIDPAR_ASTNODETYPE_FUNC_DECL) {
        if (node->func_decl.def_start) {
            midpar_parse_func_body(&node->func_decl, toks,
                                   node->func_decl.def_start - toks, allocs,
                                   diags);
        }
    } else if (node->type == MIDPAR_ASTNODETYPE_CLASS) {
        midpar_parse_class_def(&node->class_, toks, scope, allocs, diags);
    }
}

static mid_isize find_rcurly(mid_isize lcurly, const struct midlex_Token *toks,
                             struct mid_DiagVec *diags)
{
    mid_isize rcurly = midpar_find_twin_curly(toks, lcurly, MID_ISIZE_MAX);
    if (rcurly == -1)
        midgen_dynpush(diags,
                       middiag_expected_token_err("'}'", &toks[lcurly],
                                                  MIDDIAG_ERR_MISSING_CURLY));

    return rcurly == -1 ? lcurly : rcurly;
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

static mid_isize parse_accessspec(const struct midlex_Token *toks,
                                  mid_isize start,
                                  enum midpar_ClassAccess *out_spec,
                                  struct mid_DiagVec *diags)
{
    assert(midlex_is_accessspec(toks[start].type));
    if (out_spec) {
        if (toks[start].type == MIDLEX_TOKENTYPE_PUBLIC)
            *out_spec = MIDPAR_CLASSACCESS_PUBLIC;
        else if (toks[start].type == MIDLEX_TOKENTYPE_PRIVATE)
            *out_spec = MIDPAR_CLASSACCESS_PRIVATE;
        else
            *out_spec = MIDPAR_CLASSACCESS_PROTECTED;
    }

    mid_isize colon = start + 1;
    if (toks[colon].type != MIDLEX_TOKENTYPE_COLON) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("':'", &toks[start],
                                                  MIDDIAG_ERR_MISSING_TOKEN));
        return colon;
    }
    return colon + 1;
}

static void add_class_def(struct midpar_Class *self, struct mid_DiagVec *diags)
{
    if (!self->name)
        return;

    auto ident = midsema_deref_identptr(&self->ident);
    if (ident->def)
        midgen_dynpush(diags,
                       middiag_ident_redefined_err(self->name, self->def_start,
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));

    ident->def = MIDPAR_GET_NODE(self);
}

static struct midsema_Scope *setup_def_scope(struct midpar_Class *self,
                                             struct midpar_Allocators *allocs,
                                             struct mid_DiagVec *diags)
{
    auto def = &midsema_deref_identptr(&self->ident)->class_info.def_scope;
    if (*def) {
        midgen_dynpush(diags, middiag_ident_redefined_err(
                                  self->name, MIDPAR_GET_START(self),
                                  MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    *def = create_scope(midpar_class_parent(self), MIDPAR_GET_NODE(self),
                        allocs, MIDSEMA_SCOPETYPE_CLASS);

    return *def;
}

static void parse_decls(struct midpar_Class *self,
                        const struct midlex_Token *toks, mid_isize lcurly,
                        mid_isize rcurly, struct midpar_Allocators *allocs,
                        struct mid_DiagVec *diags)
{
    // class members are private by default,
    // struct and union members are public by default
    enum midpar_ClassAccess mode = self->type == MIDPAR_CLASSTYPE_CLASS
                                       ? MIDPAR_CLASSACCESS_PRIVATE
                                       : MIDPAR_CLASSACCESS_PUBLIC;

    auto def_scope = midsema_deref_identptr(&self->ident)->class_info.def_scope;

    for (mid_isize i = lcurly + 1; i < rcurly;) {
        if (midlex_is_accessspec(toks[i].type)) {
            i = parse_accessspec(toks, i, &mode, diags);
        } else {
            struct midpar_ASTNode *child =
                midpar_parse_node(toks, i, &i, MIDPAR_GET_NODE(self), def_scope,
                                  (struct midpar_ParseNodeFlags){
                                      .skip_def = true, .is_field = true},
                                  allocs, diags);

            midgen_dynpush(&self->childs, child);

            if (mode == MIDPAR_CLASSACCESS_PUBLIC)
                midgen_dynpush(&self->pub_childs, child);
            else if (mode == MIDPAR_CLASSACCESS_PRIVATE)
                midgen_dynpush(&self->priv_childs, child);
            else
                midgen_dynpush(&self->prot_childs, child);
        }
    }
}

static void parse_defs(struct midpar_Class *self,
                       const struct midlex_Token *toks,
                       struct midpar_Allocators *allocs,
                       struct mid_DiagVec *diags)
{
    auto def_scope = midsema_deref_identptr(&self->ident)->class_info.def_scope;

    for (mid_isize i = 0; i < self->childs.len; ++i)
        parse_node_def(self->childs.arr[i], def_scope, toks, allocs, diags);
}

static mid_isize parse_class_body(struct midpar_Class *self,
                                  const struct midlex_Token *toks,
                                  mid_isize lcurly,
                                  struct midpar_Allocators *allocs,
                                  struct mid_DiagVec *diags)
{
    // classes are parsed in 2 passes, the first pass gets all the declarations
    // while the second pass gets their definitions
    // this is for annoying stuff like
    // class Example {
    //    int x = y;
    //    int y;
    // };
    // among other stuff

    add_class_def(self, diags);
    setup_def_scope(self, allocs, diags);

    mid_isize rcurly = find_rcurly(lcurly, toks, diags);

    printf("CLASS DECLS PASS\n");
    parse_decls(self, toks, lcurly, rcurly, allocs, diags);

    printf("CLASS DEFS PASS\n");
    printf("%" PRIisz " pub childs, %" PRIisz " priv childs, %" PRIisz
           " prot childs\n",
           self->pub_childs.len, self->priv_childs.len, self->prot_childs.len);
    parse_defs(self, toks, allocs, diags);

    return rcurly + 1;
}

void midpar_parse_class_def(struct midpar_Class *self,
                            const struct midlex_Token *toks,
                            struct midsema_Scope *scope,
                            struct midpar_Allocators *allocs,
                            struct mid_DiagVec *diags)
{
    parse_class_body(self, toks, self->def_start - toks, allocs, diags);

    midpar_parse_var_decl_def(toks, MIDPAR_VARDECL_ENDTYPES, self->var, false,
                              scope, allocs, diags);
}

static void add_class_to_scope(struct midsema_Scope *scope,
                               struct midpar_Class *self)
{
    enum midsema_IdentType type =
        midpar_node_is_templated(MIDPAR_GET_NODE(self))
            ? MIDSEMA_IDENTTYPE_TMPLT_CLASS
            : MIDSEMA_IDENTTYPE_CLASS;
    const struct midsema_Ident *old = midsema_add_ident(
        scope, &(struct midsema_Ident){.name = self->name,
                                       .decl = MIDPAR_GET_NODE(self),
                                       .type = type});

    if (old) {
        // if this is false then the class exists across multiple scopes which
        // is bad
        assert(old->parent == self->ident.parent);
        self->ident.idx = midsema_ident_idx(old);
    } else {
        self->ident.idx = scope->idents.len - 1;
    }
}

static mid_isize parse_class_till_instances(struct midpar_Class *self,
                                            struct midsema_Scope *parent_scope,
                                            const struct midlex_Token *toks,
                                            mid_isize start, bool skip_def,
                                            struct midpar_Allocators *allocs,
                                            struct mid_DiagVec *diags)
{
    *self = (struct midpar_Class){};
    mid_isize lcurly =
        parse_class_entry(self, toks, start, parent_scope, diags);

    if (self->name)
        add_class_to_scope(midpar_class_parent(self), self);

    if (toks[lcurly].type == MIDLEX_TOKENTYPE_SEMICOLON) {
        return lcurly;
    } else if (toks[lcurly].type != MIDLEX_TOKENTYPE_L_CURLY) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("';'", &toks[start],
                                                  MIDDIAG_ERR_MISSING_TOKEN));
        return lcurly;
    }

    self->has_def = true;
    self->def_start = &toks[lcurly];

    if (skip_def) {
        return find_rcurly(lcurly, toks, diags) + 1;
    } else {
        return parse_class_body(self, toks, lcurly, allocs, diags);
    }
}

static mid_isize parse_class_instances(struct midpar_Class *self,
                                       struct midsema_Scope *parent_scope,
                                       const struct midpar_TypeStorQual *squals,
                                       const struct midpar_TypeDataQual *dquals,
                                       const struct midlex_Token *toks,
                                       mid_isize start, bool skip_def,
                                       struct midpar_Allocators *allocs,
                                       struct mid_DiagVec *diags)
{
    midgen_bumpcalloc(&allocs->ast, (struct midpar_ASTNode **)&self->var);
    MIDPAR_GET_PARENT(self->var) = MIDPAR_GET_NODE(self);
    MIDPAR_GET_START(self->var) = &toks[start];
    MIDPAR_GET_TYPE(self->var) = MIDPAR_ASTNODETYPE_VAR_DECL;

    auto base = midsema_node_type(MIDPAR_GET_NODE(self), parent_scope);
    base.squals = *squals;
    base.dquals.arr[0] = *dquals;

    mid_isize end = midpar_parse_var_decl_inst_list(
        toks, start, MIDPAR_VARDECL_ENDTYPES, &base, &self->var->insts,
        self->var, parent_scope,
        (struct midpar_ParseVarDeclFlags){.add_to_scope = true,
                                          .skip_init = skip_def},
        allocs, diags);

    midpar_Type_deinit(&base);
    return end;
}

mid_isize midpar_parse_class(struct midpar_Class *self,
                             struct midsema_Scope *parent_scope,
                             const struct midlex_Token *toks, mid_isize start,
                             bool skip_def, struct midpar_Allocators *allocs,
                             struct mid_DiagVec *diags)
{
    struct midpar_TypeStorQual squals = {};
    struct midpar_TypeDataQual dquals = {};
    start = midpar_parse_quals(toks, start, &squals, &dquals);

    mid_isize body_end = parse_class_till_instances(
        self, parent_scope, toks, start, skip_def, allocs, diags);
    if (toks[body_end].type == MIDLEX_TOKENTYPE_SEMICOLON)
        return body_end;

    return parse_class_instances(self, parent_scope, &squals, &dquals, toks,
                                 body_end, skip_def, allocs, diags);
}

bool midpar_is_field_pub(const struct midpar_Class *self,
                         const struct midpar_ASTNode *child)
{
    for (mid_isize i = 0; i < self->pub_childs.len; ++i) {
        if (child == self->pub_childs.arr[i])
            return true;
    }

    return false;
}

bool midpar_is_field_priv(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child)
{
    for (mid_isize i = 0; i < self->priv_childs.len; ++i) {
        if (child == self->priv_childs.arr[i])
            return true;
    }

    return false;
}

bool midpar_is_field_prot(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child)
{
    for (mid_isize i = 0; i < self->prot_childs.len; ++i) {
        if (child == self->prot_childs.arr[i])
            return true;
    }

    return false;
}

enum midpar_ClassAccess midpar_field_access(const struct midpar_Class *self,
                                            const struct midpar_ASTNode *child)
{
    if (midpar_is_field_pub(self, child))
        return MIDPAR_CLASSACCESS_PUBLIC;
    else if (midpar_is_field_priv(self, child))
        return MIDPAR_CLASSACCESS_PRIVATE;
    else if (midpar_is_field_prot(self, child))
        return MIDPAR_CLASSACCESS_PROTECTED;
    else
        MID_CRASH("child isn't in class");
}

mid_isize midpar_find_field(const struct midpar_Class *self, const char *name)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL &&
            child->type != MIDPAR_ASTNODETYPE_FUNC_DECL)
            continue;

        if (child->type == MIDPAR_ASTNODETYPE_VAR_DECL) {
            if (midpar_decl_inst_of_name(&child->var_decl, name))
                return i;
        } else {
            if (!strcmp(child->func_decl.name, name))
                return i;
        }
    }

    return -1;
}

struct midpar_FuncDeclPVec midpar_class_ctors(const struct midpar_Class *self)
{
    struct midpar_FuncDeclPVec ret = {};

    for (mid_isize i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type != MIDPAR_ASTNODETYPE_FUNC_DECL)
            continue;
        if (!child->func_decl.is_tor || child->func_decl.is_dtor)
            continue;

        midgen_dynpush(&ret, &child->func_decl);
    }

    return ret;
}
