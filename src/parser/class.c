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
#include "parser/end_types.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <stdlib.h>
#include <string.h>

void Parser_Class_deinit(struct Parser_Class *self)
{
    gen_dyndeinit(&self->childs);
    gen_dyndeinit(&self->pub_childs);
    gen_dyndeinit(&self->priv_childs);
    gen_dyndeinit(&self->prot_childs);
}

struct Sema_Ident *Parser_class_ident(const struct Parser_Class *self)
{
    assert(self->ident_idx != -1);
    return &self->parent->idents.arr[self->ident_idx];
}

// parses the inheritance part of a class
// class SuperHuman : Human { ... };
//                  ^       ^
//                colon   return
static isize_t parse_class_inheritance(struct Parser_Class *self,
                                       const struct Lexer_Token *toks,
                                       isize_t colon, struct DiagVec *diags)
{
    isize_t ident = colon + 1;
    if (toks[ident].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, Diag_expected_token_err("identifier", &toks[colon],
                                                   ERRORTYPE_MISSING_TOKEN));
        return ident;
    }

    const char *super_name = toks[ident].ident;
    struct Parser_ASTNode *super =
        Sema_find_ident_const(self->parent, super_name, NULL)->def;

    if (!super)
        gen_dynpush(
            diags, ((struct Diag){
                       .pos = toks[ident].pos,
                       .line = toks[ident].line,
                       .msg = Print_fmt_to_str("'%s' is undefined", super_name),
                       .err = ERRORTYPE_BAD_SUPERCLASS,
                       .is_err = true,
                   }));
    else if (super->type != PARSER_ASTNODETYPE_CLASS)
        gen_dynpush(diags, ((struct Diag){
                               .pos = toks[ident].pos,
                               .line = toks[ident].line,
                               .msg = Print_fmt_to_str(
                                   "'%s' is not a defined class", super_name),
                               .err = ERRORTYPE_BAD_SUPERCLASS,
                               .is_err = true,
                           }));
    else
        gen_dynpush(&self->supers, super);

    return ident + 1;
}

static enum Parser_ClassType parse_class_type(const struct Lexer_Token *toks,
                                              isize_t start)
{
    if (toks[start].type == LEXER_TOKENTYPE_UNION) {
        return PARSER_CLASSTYPE_UNION;
    } else if (toks[start].type == LEXER_TOKENTYPE_STRUCT) {
        return PARSER_CLASSTYPE_STRUCT;
    } else if (toks[start].type == LEXER_TOKENTYPE_CLASS) {
        return PARSER_CLASSTYPE_CLASS;
    } else {
        CRASH("tried to parse something that isn't a class");
    }
}

static isize_t parse_class_entry(struct Parser_Class *self,
                                 const struct Lexer_Token *toks, isize_t start,
                                 struct Sema_Scope *parent_scope,
                                 struct DiagVec *diags)
{
    self->type = parse_class_type(toks, start);

    isize_t ident = start + 1;
    self->parent =
        Parser_parse_scope_res(toks, ident, &ident, parent_scope, diags);
    if (toks[ident].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, Diag_expected_token_err("identifier", &toks[start],
                                                   ERRORTYPE_MISSING_TOKEN));
        --ident;
        self->name = "INVALID-NAME";
    } else {
        self->name = toks[ident].ident;
    }

    isize_t end = ident + 1;
    if (toks[end].type == LEXER_TOKENTYPE_COLON)
        end = parse_class_inheritance(self, toks, end, diags);

    return end;
}

static void parse_node_def(struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    if (node->type == PARSER_ASTNODETYPE_VAR_DECL) {
        Parser_parse_var_decl_def(toks, PARSER_VARDECL_ENDTYPES,
                                  &node->var_decl, scope, allocs, diags);
    } else if (node->type == PARSER_ASTNODETYPE_FUNC_DECL) {
        if (node->func_decl.def_start) {
            Parser_parse_func_body(toks, node->func_decl.def_start - toks,
                                   &node->func_decl, node, allocs, diags);
        }
    } else if (node->type == PARSER_ASTNODETYPE_CLASS) {
        Parser_parse_class_def(&node->class_, node, toks, scope, allocs, diags);
    }
}

static isize_t find_rcurly(isize_t lcurly, const struct Lexer_Token *toks,
                           struct DiagVec *diags)
{
    isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
    if (rcurly == -1)
        gen_dynpush(diags, Diag_expected_token_err("'}'", &toks[lcurly],
                                                   ERRORTYPE_MISSING_CURLY));

    return rcurly == -1 ? lcurly : rcurly;
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

static isize_t parse_accessspec(const struct Lexer_Token *toks, isize_t start,
                                enum Parser_ClassAccess *out_spec,
                                struct DiagVec *diags)
{
    assert(Lexer_is_accessspec(toks[start].type));
    if (out_spec) {
        if (toks[start].type == LEXER_TOKENTYPE_PUBLIC)
            *out_spec = PARSER_CLASSACCESS_PUBLIC;
        else if (toks[start].type == LEXER_TOKENTYPE_PRIVATE)
            *out_spec = PARSER_CLASSACCESS_PRIVATE;
        else
            *out_spec = PARSER_CLASSACCESS_PROTECTED;
    }

    isize_t colon = start + 1;
    if (toks[colon].type != LEXER_TOKENTYPE_COLON) {
        gen_dynpush(diags, Diag_expected_token_err("':'", &toks[start],
                                                   ERRORTYPE_MISSING_TOKEN));
        return colon;
    }
    return colon + 1;
}

static void add_class_def(struct Parser_Class *self,
                          struct Parser_ASTNode *node, struct DiagVec *diags)
{
    if (!self->name)
        return;

    auto ident = Parser_class_ident(self);
    if (ident->def)
        gen_dynpush(diags, Diag_ident_redefined_err(self->name, self->def_start,
                                                    ERRORTYPE_BAD_IDENTIFIER));

    ident->def = node;
}

static struct Sema_Scope *setup_def_scope(struct Parser_Class *self,
                                          struct Parser_ASTNode *node,
                                          struct Parser_Allocators *allocs,
                                          struct DiagVec *diags)
{
    auto def = &Parser_class_ident(self)->class_info.def_scope;
    if (*def) {
        gen_dynpush(diags, Diag_ident_redefined_err(self->name, node->start,
                                                    ERRORTYPE_BAD_IDENTIFIER));
    }

    *def = create_scope(self->parent, node, allocs, SEMA_SCOPETYPE_CLASS);

    return *def;
}

static void parse_decls(struct Parser_Class *self, struct Parser_ASTNode *node,
                        const struct Lexer_Token *toks, isize_t lcurly,
                        isize_t rcurly, struct Parser_Allocators *allocs,
                        struct DiagVec *diags)
{
    // class members are private by default,
    // struct and union members are public by default
    enum Parser_ClassAccess mode = self->type == PARSER_CLASSTYPE_CLASS
                                       ? PARSER_CLASSACCESS_PRIVATE
                                       : PARSER_CLASSACCESS_PUBLIC;

    auto def_scope = Parser_class_ident(self)->class_info.def_scope;

    for (isize_t i = lcurly + 1; i < rcurly;) {
        if (Lexer_is_accessspec(toks[i].type)) {
            i = parse_accessspec(toks, i, &mode, diags);
        } else {
            struct Parser_ASTNode *child = Parser_parse_node(
                toks, i, &i, node, def_scope, true, allocs, diags);

            gen_dynpush(&self->childs, child);

            if (mode == PARSER_CLASSACCESS_PUBLIC)
                gen_dynpush(&self->pub_childs, child);
            else if (mode == PARSER_CLASSACCESS_PRIVATE)
                gen_dynpush(&self->priv_childs, child);
            else
                gen_dynpush(&self->prot_childs, child);
        }
    }
}

static void parse_defs(struct Parser_Class *self,
                       const struct Lexer_Token *toks,
                       struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    auto def_scope = Parser_class_ident(self)->class_info.def_scope;

    for (isize_t i = 0; i < self->childs.len; ++i)
        parse_node_def(self->childs.arr[i], def_scope, toks, allocs, diags);
}

static isize_t parse_class_body(struct Parser_Class *self,
                                struct Parser_ASTNode *node,
                                const struct Lexer_Token *toks, isize_t lcurly,
                                struct Parser_Allocators *allocs,
                                struct DiagVec *diags)
{
    // classes are parsed in 2 passes, the first pass gets all the declarations
    // while the second pass gets their definitions
    // this is for annoying stuff like
    // class Example {
    //    int x = y;
    //    int y;
    // };
    // among other stuff

    add_class_def(self, node, diags);
    setup_def_scope(self, node, allocs, diags);

    isize_t rcurly = find_rcurly(lcurly, toks, diags);

    printf("CLASS DECLS PASS\n");
    parse_decls(self, node, toks, lcurly, rcurly, allocs, diags);

    printf("CLASS DEFS PASS\n");
    printf("%" PRIisz " pub childs, %" PRIisz " priv childs, %" PRIisz
           " prot childs\n",
           self->pub_childs.len, self->priv_childs.len, self->prot_childs.len);
    parse_defs(self, toks, allocs, diags);

    return rcurly + 1;
}

void Parser_parse_class_def(struct Parser_Class *self,
                            struct Parser_ASTNode *node,
                            const struct Lexer_Token *toks,
                            struct Sema_Scope *scope,
                            struct Parser_Allocators *allocs,
                            struct DiagVec *diags)
{
    parse_class_body(self, node, toks, self->def_start - toks, allocs, diags);

    Parser_parse_var_decl_def(toks, PARSER_VARDECL_ENDTYPES,
                              &self->var_decl->var_decl, scope, allocs, diags);
}

static void add_class_to_scope(struct Sema_Scope *scope,
                               struct Parser_Class *self,
                               struct Parser_ASTNode *node)
{
    const struct Sema_Ident *old = Sema_add_ident(
        scope, &(struct Sema_Ident){.name = self->name,
                                    .decl = node,
                                    .type = SEMA_IDENTTYPE_CLASS});

    if (old)
        self->ident_idx = old - scope->idents.arr;
    else
        self->ident_idx = scope->idents.len - 1;
}

static isize_t parse_class_till_instances(struct Parser_Class *self,
                                          struct Parser_ASTNode *node,
                                          struct Sema_Scope *parent_scope,
                                          const struct Lexer_Token *toks,
                                          isize_t start, bool skip_def,
                                          struct Parser_Allocators *allocs,
                                          struct DiagVec *diags)
{
    *self = (struct Parser_Class){.ident_idx = -1};
    isize_t lcurly = parse_class_entry(self, toks, start, parent_scope, diags);

    if (self->name)
        add_class_to_scope(self->parent, self, node);

    if (toks[lcurly].type == LEXER_TOKENTYPE_SEMICOLON) {
        return lcurly;
    } else if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY) {
        gen_dynpush(diags, Diag_expected_token_err("';'", &toks[start],
                                                   ERRORTYPE_MISSING_TOKEN));
        return lcurly;
    }

    self->has_def = true;
    self->def_start = &toks[lcurly];

    if (skip_def) {
        return find_rcurly(lcurly, toks, diags) + 1;
    } else {
        return parse_class_body(self, node, toks, lcurly, allocs, diags);
    }
}

static isize_t parse_class_instances(struct Parser_Class *self,
                                     struct Parser_ASTNode *node,
                                     struct Sema_Scope *parent_scope,
                                     const struct Lexer_Token *toks,
                                     isize_t start, bool skip_def,
                                     struct Parser_Allocators *allocs,
                                     struct DiagVec *diags)
{
    gen_bumpcalloc(&allocs->ast, &self->var_decl);
    self->var_decl->parent = node;
    self->var_decl->start = &toks[start];
    self->var_decl->type = PARSER_ASTNODETYPE_VAR_DECL;

    auto base = Sema_node_type(node, parent_scope, NULL);

    isize_t end = Parser_parse_var_decl_inst_list(
        toks, start, PARSER_VARDECL_ENDTYPES, &base,
        &self->var_decl->var_decl.insts, self->var_decl, parent_scope, true,
        false, skip_def, allocs, diags);

    Parser_Type_deinit(&base);
    return end;
}

isize_t Parser_parse_class(struct Parser_Class *self,
                           struct Parser_ASTNode *node,
                           struct Sema_Scope *parent_scope,
                           const struct Lexer_Token *toks, isize_t start,
                           bool skip_def, struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    isize_t body_end = parse_class_till_instances(
        self, node, parent_scope, toks, start, skip_def, allocs, diags);
    if (toks[body_end].type == LEXER_TOKENTYPE_SEMICOLON)
        return body_end;

    return parse_class_instances(self, node, parent_scope, toks, body_end,
                                 skip_def, allocs, diags);
}

bool Parser_is_field_pub(const struct Parser_Class *self,
                         const struct Parser_ASTNode *child)
{
    for (isize_t i = 0; i < self->pub_childs.len; ++i) {
        if (child == self->pub_childs.arr[i])
            return true;
    }

    return false;
}

bool Parser_is_field_priv(const struct Parser_Class *self,
                          const struct Parser_ASTNode *child)
{
    for (isize_t i = 0; i < self->priv_childs.len; ++i) {
        if (child == self->priv_childs.arr[i])
            return true;
    }

    return false;
}

bool Parser_is_field_prot(const struct Parser_Class *self,
                          const struct Parser_ASTNode *child)
{
    for (isize_t i = 0; i < self->prot_childs.len; ++i) {
        if (child == self->prot_childs.arr[i])
            return true;
    }

    return false;
}

enum Parser_ClassAccess Parser_field_access(const struct Parser_Class *self,
                                            const struct Parser_ASTNode *child)
{
    if (Parser_is_field_pub(self, child))
        return PARSER_CLASSACCESS_PUBLIC;
    else if (Parser_is_field_priv(self, child))
        return PARSER_CLASSACCESS_PRIVATE;
    else if (Parser_is_field_prot(self, child))
        return PARSER_CLASSACCESS_PROTECTED;
    else
        CRASH("child isn't in class");
}

isize_t Parser_find_field(const struct Parser_Class *self, const char *name)
{
    for (isize_t i = 0; i < self->childs.len; ++i) {
        auto child = self->childs.arr[i];

        if (child->type != PARSER_ASTNODETYPE_VAR_DECL &&
            child->type != PARSER_ASTNODETYPE_FUNC_DECL)
            continue;

        if (child->type == PARSER_ASTNODETYPE_VAR_DECL) {
            if (Parser_decl_inst_of_name(&child->var_decl, name))
                return i;
        } else {
            if (!strcmp(child->func_decl.name, name))
                return i;
        }
    }

    return -1;
}
