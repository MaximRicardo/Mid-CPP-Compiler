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

void MidParser_Class_deinit(struct MidParser_Class *self)
{
    MidGen_dyndeinit(&self->childs);
    MidGen_dyndeinit(&self->pub_childs);
    MidGen_dyndeinit(&self->priv_childs);
    MidGen_dyndeinit(&self->prot_childs);
}

// takes an array of ptrs to nodes in old_nodes and transforms each ptr to the
// one at the same idx in new_nodes
static struct MidParser_ASTNodePVec
transf_node_ptrs(const struct MidParser_ASTNodePVec *ptrs,
                 struct MidParser_ASTNode *const *old_nodes,
                 struct MidParser_ASTNode *const *new_nodes, mid_isize n_nodes)
{
    struct MidParser_ASTNodePVec ret = {};
    MidGen_dynreserve(&ret, ptrs->len);

    for (mid_isize p_i = 0; p_i < ptrs->len; ++p_i) {
        mid_isize n_i;
        for (n_i = 0; n_i < n_nodes; ++n_i) {
            if (old_nodes[n_i] == ptrs->arr[p_i])
                break;
        }

        if (n_i == n_nodes)
            MID_CRASH("ptr not in old_nodes");

        MidGen_dynpush(&ret, new_nodes[n_i]);
    }

    return ret;
}

void MidParser_copy_class(struct MidParser_Class *dest,
                          const struct MidParser_Class *src,
                          struct MidSema_Scope *dest_scope,
                          struct MidParser_Allocators *allocs)
{
    *dest = *src;

    auto old_ident = MidSema_add_ident_copy(
        dest_scope, MidSema_deref_identptr(&src->ident), false, allocs);
    if (old_ident)
        dest->ident = MidSema_create_identptr(old_ident);
    else
        dest->ident = MidSema_identptr_to_last(dest_scope);

    if (dest->childs.len > 0) {
        struct MidSema_Scope *child_scope;
        MidGen_bumpmalloc(&allocs->scope, &child_scope);
        *child_scope =
            (struct MidSema_Scope){.parent = MidParser_class_parent(dest),
                                   .node = MIDPARSER_GET_NODE(dest),
                                   .type = MIDSEMA_SCOPETYPE_CLASS};
        MidSema_deref_identptr(&dest->ident)->class_info.def_scope =
            child_scope;

        dest->childs = MidParser_copy_nodepvec(
            &src->childs, MIDPARSER_GET_NODE(dest), child_scope, allocs);
        dest->pub_childs = transf_node_ptrs(&src->pub_childs, src->childs.arr,
                                            dest->childs.arr, src->childs.len);
        dest->priv_childs = transf_node_ptrs(&src->priv_childs, src->childs.arr,
                                             dest->childs.arr, src->childs.len);
        dest->prot_childs = transf_node_ptrs(&src->prot_childs, src->childs.arr,
                                             dest->childs.arr, src->childs.len);
    }

    if (src->var) {
        MidGen_bumpmalloc(&allocs->ast, (void **)&dest->var);
        MidParser_copy_node(
            MIDPARSER_GET_NODE(dest->var), MIDPARSER_GET_NODE(src->var),
            MIDPARSER_GET_NODE(dest), MidParser_class_parent(dest), allocs);
    }
}

struct MidSema_Scope *MidParser_class_parent(const struct MidParser_Class *self)
{
    return self->ident.parent;
}

// parses the inheritance part of a class
// class SuperHuman : Human { ... };
//                  ^       ^
//                colon   return
static mid_isize parse_class_inheritance(struct MidParser_Class *self,
                                       const struct MidLexer_Token *toks,
                                       mid_isize colon,
                                       struct MidDiag_DiagVec *diags)
{
    mid_isize ident = colon + 1;
    if (toks[ident].type != MIDLEXER_TOKENTYPE_IDENTIFIER) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("identifier", &toks[colon],
                                               MIDDIAG_ERR_MISSING_TOKEN));
        return ident;
    }

    const char *super_name = toks[ident].ident;
    struct MidParser_Class *super =
        &MidSema_find_ident_const(MidParser_class_parent(self), super_name)
             ->def->class_;

    if (!super)
        MidGen_dynpush(diags, ((struct MidDiag_Diag){
                               .pos = toks[ident].pos,
                               .line = toks[ident].line,
                               .msg = MidPrint_fmt_to_str("'%s' is undefined",
                                                          super_name),
                               .err = MIDDIAG_ERR_BAD_SUPERCLASS,
                               .type = MIDDIAG_TYPE_ERROR,
                           }));
    else if (MIDPARSER_GET_TYPE(super) != MIDPARSER_ASTNODETYPE_CLASS)
        MidGen_dynpush(diags, ((struct MidDiag_Diag){
                               .pos = toks[ident].pos,
                               .line = toks[ident].line,
                               .msg = MidPrint_fmt_to_str(
                                   "'%s' is not a defined class", super_name),
                               .err = MIDDIAG_ERR_BAD_SUPERCLASS,
                               .type = MIDDIAG_TYPE_ERROR,
                           }));
    else
        MidGen_dynpush(&self->supers, super);

    return ident + 1;
}

static enum MidParser_ClassType
parse_class_type(const struct MidLexer_Token *toks, mid_isize start)
{
    if (toks[start].type == MIDLEXER_TOKENTYPE_UNION) {
        return MIDPARSER_CLASSTYPE_UNION;
    } else if (toks[start].type == MIDLEXER_TOKENTYPE_STRUCT) {
        return MIDPARSER_CLASSTYPE_STRUCT;
    } else if (toks[start].type == MIDLEXER_TOKENTYPE_CLASS) {
        return MIDPARSER_CLASSTYPE_CLASS;
    } else {
        MID_CRASH("tried to parse something that isn't a class");
    }
}

static mid_isize parse_class_entry(struct MidParser_Class *self,
                                 const struct MidLexer_Token *toks,
                                 mid_isize start,
                                 struct MidSema_Scope *parent_scope,
                                 struct MidDiag_DiagVec *diags)
{
    self->type = parse_class_type(toks, start);

    mid_isize ident = start + 1;
    self->ident.parent =
        MidParser_parse_scope_res(toks, ident, &ident, parent_scope, diags);
    if (toks[ident].type != MIDLEXER_TOKENTYPE_IDENTIFIER) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("identifier", &toks[start],
                                               MIDDIAG_ERR_MISSING_TOKEN));
        --ident;
        self->name = "INVALID-NAME";
    } else {
        self->name = toks[ident].ident;
    }

    mid_isize end = ident + 1;
    if (toks[end].type == MIDLEXER_TOKENTYPE_COLON)
        end = parse_class_inheritance(self, toks, end, diags);

    return end;
}

static void parse_node_def(struct MidParser_ASTNode *node,
                           struct MidSema_Scope *scope,
                           const struct MidLexer_Token *toks,
                           struct MidParser_Allocators *allocs,
                           struct MidDiag_DiagVec *diags)
{
    if (node->type == MIDPARSER_ASTNODETYPE_VAR_DECL) {
        MidParser_parse_var_decl_def(toks, MIDPARSER_VARDECL_ENDTYPES,
                                     &node->var_decl, false, scope, allocs,
                                     diags);
    } else if (node->type == MIDPARSER_ASTNODETYPE_FUNC_DECL) {
        if (node->func_decl.def_start) {
            MidParser_parse_func_body(&node->func_decl, toks,
                                      node->func_decl.def_start - toks, allocs,
                                      diags);
        }
    } else if (node->type == MIDPARSER_ASTNODETYPE_CLASS) {
        MidParser_parse_class_def(&node->class_, toks, scope, allocs, diags);
    }
}

static mid_isize find_rcurly(mid_isize lcurly, const struct MidLexer_Token *toks,
                           struct MidDiag_DiagVec *diags)
{
    mid_isize rcurly = MidParser_find_twin_curly(toks, lcurly, MID_ISIZE_MAX);
    if (rcurly == -1)
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("'}'", &toks[lcurly],
                                               MIDDIAG_ERR_MISSING_CURLY));

    return rcurly == -1 ? lcurly : rcurly;
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

static mid_isize parse_accessspec(const struct MidLexer_Token *toks,
                                mid_isize start,
                                enum MidParser_ClassAccess *out_spec,
                                struct MidDiag_DiagVec *diags)
{
    assert(MidLexer_is_accessspec(toks[start].type));
    if (out_spec) {
        if (toks[start].type == MIDLEXER_TOKENTYPE_PUBLIC)
            *out_spec = MIDPARSER_CLASSACCESS_PUBLIC;
        else if (toks[start].type == MIDLEXER_TOKENTYPE_PRIVATE)
            *out_spec = MIDPARSER_CLASSACCESS_PRIVATE;
        else
            *out_spec = MIDPARSER_CLASSACCESS_PROTECTED;
    }

    mid_isize colon = start + 1;
    if (toks[colon].type != MIDLEXER_TOKENTYPE_COLON) {
        MidGen_dynpush(diags, MidDiag_expected_token_err(
                               "':'", &toks[start], MIDDIAG_ERR_MISSING_TOKEN));
        return colon;
    }
    return colon + 1;
}

static void add_class_def(struct MidParser_Class *self,
                          struct MidDiag_DiagVec *diags)
{
    if (!self->name)
        return;

    auto ident = MidSema_deref_identptr(&self->ident);
    if (ident->def)
        MidGen_dynpush(diags,
                    MidDiag_ident_redefined_err(self->name, self->def_start,
                                                MIDDIAG_ERR_BAD_IDENTIFIER));

    ident->def = MIDPARSER_GET_NODE(self);
}

static struct MidSema_Scope *
setup_def_scope(struct MidParser_Class *self,
                struct MidParser_Allocators *allocs,
                struct MidDiag_DiagVec *diags)
{
    auto def = &MidSema_deref_identptr(&self->ident)->class_info.def_scope;
    if (*def) {
        MidGen_dynpush(diags, MidDiag_ident_redefined_err(
                               self->name, MIDPARSER_GET_START(self),
                               MIDDIAG_ERR_BAD_IDENTIFIER));
    }

    *def = create_scope(MidParser_class_parent(self), MIDPARSER_GET_NODE(self),
                        allocs, MIDSEMA_SCOPETYPE_CLASS);

    return *def;
}

static void parse_decls(struct MidParser_Class *self,
                        const struct MidLexer_Token *toks, mid_isize lcurly,
                        mid_isize rcurly, struct MidParser_Allocators *allocs,
                        struct MidDiag_DiagVec *diags)
{
    // class members are private by default,
    // struct and union members are public by default
    enum MidParser_ClassAccess mode = self->type == MIDPARSER_CLASSTYPE_CLASS
                                          ? MIDPARSER_CLASSACCESS_PRIVATE
                                          : MIDPARSER_CLASSACCESS_PUBLIC;

    auto def_scope = MidSema_deref_identptr(&self->ident)->class_info.def_scope;

    for (mid_isize i = lcurly + 1; i < rcurly;) {
        if (MidLexer_is_accessspec(toks[i].type)) {
            i = parse_accessspec(toks, i, &mode, diags);
        } else {
            struct MidParser_ASTNode *child = MidParser_parse_node(
                toks, i, &i, MIDPARSER_GET_NODE(self), def_scope,
                (struct MidParser_ParseNodeFlags){.skip_def = true,
                                                  .is_field = true},
                allocs, diags);

            MidGen_dynpush(&self->childs, child);

            if (mode == MIDPARSER_CLASSACCESS_PUBLIC)
                MidGen_dynpush(&self->pub_childs, child);
            else if (mode == MIDPARSER_CLASSACCESS_PRIVATE)
                MidGen_dynpush(&self->priv_childs, child);
            else
                MidGen_dynpush(&self->prot_childs, child);
        }
    }
}

static void parse_defs(struct MidParser_Class *self,
                       const struct MidLexer_Token *toks,
                       struct MidParser_Allocators *allocs,
                       struct MidDiag_DiagVec *diags)
{
    auto def_scope = MidSema_deref_identptr(&self->ident)->class_info.def_scope;

    for (mid_isize i = 0; i < self->childs.len; ++i)
        parse_node_def(self->childs.arr[i], def_scope, toks, allocs, diags);
}

static mid_isize parse_class_body(struct MidParser_Class *self,
                                const struct MidLexer_Token *toks,
                                mid_isize lcurly,
                                struct MidParser_Allocators *allocs,
                                struct MidDiag_DiagVec *diags)
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

void MidParser_parse_class_def(struct MidParser_Class *self,
                               const struct MidLexer_Token *toks,
                               struct MidSema_Scope *scope,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags)
{
    parse_class_body(self, toks, self->def_start - toks, allocs, diags);

    MidParser_parse_var_decl_def(toks, MIDPARSER_VARDECL_ENDTYPES, self->var,
                                 false, scope, allocs, diags);
}

static void add_class_to_scope(struct MidSema_Scope *scope,
                               struct MidParser_Class *self)
{
    enum MidSema_IdentType type =
        MidParser_node_is_templated(MIDPARSER_GET_NODE(self))
            ? MIDSEMA_IDENTTYPE_TMPLT_CLASS
            : MIDSEMA_IDENTTYPE_CLASS;
    const struct MidSema_Ident *old = MidSema_add_ident(
        scope, &(struct MidSema_Ident){.name = self->name,
                                       .decl = MIDPARSER_GET_NODE(self),
                                       .type = type});

    if (old) {
        // if this is false then the class exists across multiple scopes which
        // is bad
        assert(old->parent == self->ident.parent);
        self->ident.idx = MidSema_ident_idx(old);
    } else {
        self->ident.idx = scope->idents.len - 1;
    }
}

static mid_isize parse_class_till_instances(struct MidParser_Class *self,
                                          struct MidSema_Scope *parent_scope,
                                          const struct MidLexer_Token *toks,
                                          mid_isize start, bool skip_def,
                                          struct MidParser_Allocators *allocs,
                                          struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_Class){};
    mid_isize lcurly = parse_class_entry(self, toks, start, parent_scope, diags);

    if (self->name)
        add_class_to_scope(MidParser_class_parent(self), self);

    if (toks[lcurly].type == MIDLEXER_TOKENTYPE_SEMICOLON) {
        return lcurly;
    } else if (toks[lcurly].type != MIDLEXER_TOKENTYPE_L_CURLY) {
        MidGen_dynpush(diags, MidDiag_expected_token_err(
                               "';'", &toks[start], MIDDIAG_ERR_MISSING_TOKEN));
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

static mid_isize parse_class_instances(
    struct MidParser_Class *self, struct MidSema_Scope *parent_scope,
    const struct MidParser_TypeStorQual *squals,
    const struct MidParser_TypeDataQual *dquals,
    const struct MidLexer_Token *toks, mid_isize start, bool skip_def,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    MidGen_bumpcalloc(&allocs->ast, (struct MidParser_ASTNode **)&self->var);
    MIDPARSER_GET_PARENT(self->var) = MIDPARSER_GET_NODE(self);
    MIDPARSER_GET_START(self->var) = &toks[start];
    MIDPARSER_GET_TYPE(self->var) = MIDPARSER_ASTNODETYPE_VAR_DECL;

    auto base = MidSema_node_type(MIDPARSER_GET_NODE(self), parent_scope);
    base.squals = *squals;
    base.dquals.arr[0] = *dquals;

    mid_isize end = MidParser_parse_var_decl_inst_list(
        toks, start, MIDPARSER_VARDECL_ENDTYPES, &base, &self->var->insts,
        self->var, parent_scope,
        (struct MidParser_ParseVarDeclFlags){.add_to_scope = true,
                                             .skip_init = skip_def},
        allocs, diags);

    MidParser_Type_deinit(&base);
    return end;
}

mid_isize MidParser_parse_class(struct MidParser_Class *self,
                              struct MidSema_Scope *parent_scope,
                              const struct MidLexer_Token *toks, mid_isize start,
                              bool skip_def,
                              struct MidParser_Allocators *allocs,
                              struct MidDiag_DiagVec *diags)
{
    struct MidParser_TypeStorQual squals = {};
    struct MidParser_TypeDataQual dquals = {};
    start = MidParser_parse_quals(toks, start, &squals, &dquals);

    mid_isize body_end = parse_class_till_instances(
        self, parent_scope, toks, start, skip_def, allocs, diags);
    if (toks[body_end].type == MIDLEXER_TOKENTYPE_SEMICOLON)
        return body_end;

    return parse_class_instances(self, parent_scope, &squals, &dquals, toks,
                                 body_end, skip_def, allocs, diags);
}

bool MidParser_is_field_pub(const struct MidParser_Class *self,
                            const struct MidParser_ASTNode *child)
{
    for (mid_isize i = 0; i < self->pub_childs.len; ++i) {
        if (child == self->pub_childs.arr[i])
            return true;
    }

    return false;
}

bool MidParser_is_field_priv(const struct MidParser_Class *self,
                             const struct MidParser_ASTNode *child)
{
    for (mid_isize i = 0; i < self->priv_childs.len; ++i) {
        if (child == self->priv_childs.arr[i])
            return true;
    }

    return false;
}

bool MidParser_is_field_prot(const struct MidParser_Class *self,
                             const struct MidParser_ASTNode *child)
{
    for (mid_isize i = 0; i < self->prot_childs.len; ++i) {
        if (child == self->prot_childs.arr[i])
            return true;
    }

    return false;
}

enum MidParser_ClassAccess
MidParser_field_access(const struct MidParser_Class *self,
                       const struct MidParser_ASTNode *child)
{
    if (MidParser_is_field_pub(self, child))
        return MIDPARSER_CLASSACCESS_PUBLIC;
    else if (MidParser_is_field_priv(self, child))
        return MIDPARSER_CLASSACCESS_PRIVATE;
    else if (MidParser_is_field_prot(self, child))
        return MIDPARSER_CLASSACCESS_PROTECTED;
    else
        MID_CRASH("child isn't in class");
}

mid_isize MidParser_find_field(const struct MidParser_Class *self,
                             const char *name)
{
    for (mid_isize i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type != MIDPARSER_ASTNODETYPE_VAR_DECL &&
            child->type != MIDPARSER_ASTNODETYPE_FUNC_DECL)
            continue;

        if (child->type == MIDPARSER_ASTNODETYPE_VAR_DECL) {
            if (MidParser_decl_inst_of_name(&child->var_decl, name))
                return i;
        } else {
            if (!strcmp(child->func_decl.name, name))
                return i;
        }
    }

    return -1;
}

struct MidParser_FuncDeclPVec
MidParser_class_ctors(const struct MidParser_Class *self)
{
    struct MidParser_FuncDeclPVec ret = {};

    for (mid_isize i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type != MIDPARSER_ASTNODETYPE_FUNC_DECL)
            continue;
        if (!child->func_decl.is_tor || child->func_decl.is_dtor)
            continue;

        MidGen_dynpush(&ret, &child->func_decl);
    }

    return ret;
}
