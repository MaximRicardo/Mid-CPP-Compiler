#include "class.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <stdlib.h>
#include <string.h>

void Parser_Class_deinit(struct Parser_Class *self)
{
    gen_dyndeinit(&self->pub_childs);
    gen_dyndeinit(&self->priv_childs);
    gen_dyndeinit(&self->prot_childs);
}

static struct Diag expected_token(const char *tok_name,
                                  const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("expected %s", tok_name),
        .err = ERRORTYPE_MISSING_TOKEN,
        .is_err = true,
    };
}

// parses the inheritance part of a class
// class SuperHuman : Human { ... };
//                  ^       ^
//                colon   return
static isize_t parse_class_inheritance(struct Parser_Class *self,
                                       const struct Sema_Scope *scope,
                                       const struct Lexer_Token *toks,
                                       isize_t colon, struct DiagVec *diags)
{
    isize_t ident = colon + 1;
    if (toks[ident].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, expected_token("identifier", &toks[colon]));
        return ident;
    }

    const char *super_name = toks[ident].ident;
    struct Parser_ASTNode *super =
        Sema_find_ident_const(scope, super_name)->def;

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
        self->super = super;

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
                                 const struct Sema_Scope *scope,
                                 const struct Lexer_Token *toks, isize_t start,
                                 struct DiagVec *diags)
{
    self->type = parse_class_type(toks, start);

    isize_t ident = start + 1;
    if (toks[ident].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, expected_token("identifier", &toks[start]));
        --ident;
        self->name = "INVALID-NAME";
    } else {
        self->name = toks[ident].ident;
    }

    isize_t end = ident + 1;
    if (toks[end].type == LEXER_TOKENTYPE_COLON)
        end = parse_class_inheritance(self, scope, toks, end, diags);

    return end;
}

static void parse_node_def(struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks,
                           struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    if (node->type == PARSER_ASTNODETYPE_VAR_DECL) {
        if (node->var_decl.init_start) {
            gen_bumpmalloc(&allocs->expr, &node->var_decl.init);
            *node->var_decl.init =
                Parser_parse_expr(toks, node->var_decl.init_start - toks,
                                  PARSER_DEFAULT_ENDTYPES, NULL, diags);
        }
    } else if (node->type == PARSER_ASTNODETYPE_FUNC_DECL) {
        if (node->func_decl.def_start) {
            Parser_parse_func_body(toks, node->func_decl.def_start - toks,
                                   &node->func_decl, node, allocs, diags);
        }
    } else if (node->type == PARSER_ASTNODETYPE_CLASS) {
        if (node->class_.def_start) {
            Parser_parse_class_body(&node->class_, node, scope, toks,
                                    toks - node->class_.def_start, allocs,
                                    diags);
        }
    }
}

static isize_t find_rcurly(isize_t lcurly, const struct Lexer_Token *toks,
                           struct DiagVec *diags)
{
    isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
    if (rcurly == -1)
        gen_dynpush(diags, ((struct Diag){
                               .pos = toks[lcurly].pos,
                               .line = toks[lcurly].line,
                               .msg = strdup("expected '}'"),
                               .err = ERRORTYPE_MISSING_CURLY,
                               .is_err = true,
                           }));

    return rcurly == -1 ? lcurly : rcurly;
}

static struct Sema_Scope *create_class_scope(struct Sema_Scope *scope,
                                             struct Parser_ASTNode *node,
                                             struct Parser_Allocators *allocs)
{
    struct Sema_Scope *child;
    gen_bumpmalloc(&allocs->scope, &child);
    *child = (struct Sema_Scope){
        .parent = scope, .node = node, .type = SEMA_SCOPETYPE_CLASS};
    gen_dynpush(&scope->childs, child);

    return child;
}

static void add_class_def(struct Parser_Class *self,
                          struct Parser_ASTNode *node, struct DiagVec *diags)
{
    if (!self->name)
        return;

    if (Sema_add_ident_def(self->scope, self->name, node) != 0) {
        gen_dynpush(diags,
                    ((struct Diag){
                        .pos = self->def_start->pos,
                        .line = self->def_start->line,
                        .msg = Print_fmt_to_str("'%s' redefined", self->name),
                        .err = ERRORTYPE_BAD_IDENTIFIER,
                        .is_err = true,
                    }));
    }
}

enum AccessSpec {
    ACCESSSPEC_PUBLIC,
    ACCESSSPEC_PRIVATE,
    ACCESSSPEC_PROTECTED,
};

static isize_t parse_accessspec(const struct Lexer_Token *toks, isize_t start,
                                enum AccessSpec *out_spec,
                                struct DiagVec *diags)
{
    assert(Lexer_is_accessspec(toks[start].type));
    if (out_spec) {
        if (toks[start].type == LEXER_TOKENTYPE_PUBLIC)
            *out_spec = ACCESSSPEC_PUBLIC;
        else if (toks[start].type == LEXER_TOKENTYPE_PRIVATE)
            *out_spec = ACCESSSPEC_PRIVATE;
        else
            *out_spec = ACCESSSPEC_PROTECTED;
    }

    isize_t colon = start + 1;
    if (toks[colon].type != LEXER_TOKENTYPE_COLON) {
        gen_dynpush(diags, expected_token("':'", &toks[start]));
        return colon;
    }
    return colon + 1;
}

static void parse_decls(struct Parser_Class *self, struct Parser_ASTNode *node,
                        const struct Lexer_Token *toks, isize_t lcurly,
                        isize_t rcurly, struct Parser_Allocators *allocs,
                        struct DiagVec *diags)
{
    // class members are private by default,
    // struct and union members are public by default
    enum AccessSpec mode = self->type == PARSER_CLASSTYPE_CLASS
                               ? ACCESSSPEC_PRIVATE
                               : ACCESSSPEC_PUBLIC;

    for (isize_t i = lcurly + 1; i < rcurly;) {
        if (Lexer_is_accessspec(toks[i].type)) {
            i = parse_accessspec(toks, i, &mode, diags);
        } else {
            struct Parser_ASTNode *child = Parser_parse_node(
                toks, i, &i, node, self->scope, true, allocs, diags);

            if (mode == ACCESSSPEC_PUBLIC)
                gen_dynpush(&self->pub_childs, child);
            else if (mode == ACCESSSPEC_PRIVATE)
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
    for (isize_t i = 0; i < self->pub_childs.len; ++i)
        parse_node_def(self->pub_childs.arr[i], self->scope, toks, allocs,
                       diags);

    for (isize_t i = 0; i < self->priv_childs.len; ++i)
        parse_node_def(self->priv_childs.arr[i], self->scope, toks, allocs,
                       diags);

    for (isize_t i = 0; i < self->prot_childs.len; ++i)
        parse_node_def(self->prot_childs.arr[i], self->scope, toks, allocs,
                       diags);
}

isize_t Parser_parse_class_body(struct Parser_Class *self,
                                struct Parser_ASTNode *node,
                                struct Sema_Scope *parent_scope,
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

    self->scope = create_class_scope(parent_scope, node, allocs);
    add_class_def(self, node, diags);

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

static void add_class_to_scope(struct Sema_Scope *scope, const char *name,
                               struct Parser_ASTNode *node)
{
    Sema_add_ident(scope, &(struct Sema_Ident){.name = name,
                                               .decl = node,
                                               .type = SEMA_IDENTTYPE_CLASS});
}

isize_t Parser_parse_class(struct Parser_Class *self,
                           struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           bool skip_def, struct Parser_Allocators *allocs,
                           struct DiagVec *diags)
{
    *self = (struct Parser_Class){};

    isize_t lcurly = parse_class_entry(self, scope, toks, start, diags);

    if (self->name)
        add_class_to_scope(scope, self->name, node);

    if (toks[lcurly].type == LEXER_TOKENTYPE_SEMICOLON) {
        return lcurly;
    } else if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY) {
        gen_dynpush(diags, expected_token("';'", &toks[start]));
        return lcurly;
    }

    self->has_def = true;
    self->def_start = &toks[lcurly];

    if (skip_def) {
        return find_rcurly(lcurly, toks, diags) + 1;
    } else {
        return Parser_parse_class_body(self, node, scope, toks, lcurly, allocs,
                                       diags);
    }
}
