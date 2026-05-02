#include "class.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "print.h"
#include "sema/ident.h"
#include <stdlib.h>

void Parser_Class_deinit(struct Parser_Class *self)
{
    gen_dyndeinit(&self->nodes, Parser_ASTNodeP_deinit);
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
static isize_t parse_class_inheritance(struct Parser_ASTNode *node,
                                       const struct Lexer_Token *toks,
                                       isize_t colon, struct DiagVec *diags)
{
    isize_t ident = colon + 1;
    if (toks[ident].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, expected_token("identifier", &toks[colon]));
        return ident;
    }

    const char *super_name = toks[ident].ident;
    struct Parser_ASTNode *super = Sema_ident_def(super_name, node);

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
        node->class_.super = super;

    return ident + 1;
}

static isize_t parse_class_entry(struct Parser_ASTNode *node,
                                 const struct Lexer_Token *toks, isize_t start,
                                 bool *out_is_struct, struct DiagVec *diags)
{
    struct Parser_Class *self = &node->class_;

    if (toks[start].type == LEXER_TOKENTYPE_UNION) {
        self->is_union = false;
    } else if (toks[start].type == LEXER_TOKENTYPE_STRUCT) {
        if (out_is_struct)
            *out_is_struct = true;
    } else if (toks[start].type != LEXER_TOKENTYPE_CLASS) {
        CRASH("tried to parse something that isn't a class");
    }

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
        end = parse_class_inheritance(node, toks, end, diags);

    return end;
}

static void parse_node_def(struct Parser_ASTNode *node,
                           const struct Lexer_Token *toks,
                           struct DiagVec *diags)
{
    if (node->type == PARSER_ASTNODETYPE_VAR_DECL) {
        if (node->var_decl.init_start) {
            node->var_decl.init = malloc(sizeof(*node->var_decl.init));
            *node->var_decl.init =
                Parser_parse_expr(toks, node->var_decl.init_start - toks,
                                  PARSER_DEFAULT_ENDTYPES, NULL, diags);
        }
    } else if (node->type == PARSER_ASTNODETYPE_FUNC_DECL) {
        if (node->func_decl.def_start) {
            Parser_parse_func_body(&node->func_decl, toks,
                                   node->func_decl.def_start - toks, node,
                                   diags);
        }
    }
}

static void parse_class_body(struct Parser_ASTNode *node,
                             const struct Lexer_Token *toks, isize_t l_curly,
                             isize_t r_curly, struct DiagVec *diags)
{
    // classes are parsed in 2 passes, the first pass gets all the declarations
    // while the second pass gets their definitions
    // this is for annoying stuff like
    // class Example {
    //    int x = y;
    //    int y;
    // };
    // among other stuff

    struct Parser_Class *self = &node->class_;

    printf("CLASS DECLS PASS\n");

    for (isize_t i = l_curly + 1; i < r_curly;) {
        struct Parser_ASTNode *child =
            Parser_parse_node(toks, i, &i, node, true, diags);

        gen_dynpush(&self->nodes, child);
    }

    printf("CLASS DEFS PASS\n");
    printf("n nodes = %" PRIisz "\n", self->nodes.len);

    for (isize_t i = 0; i < self->nodes.len; ++i) {
        parse_node_def(self->nodes.arr[i], toks, diags);
    }
}

isize_t Parser_parse_class(struct Parser_ASTNode *node,
                           const struct Lexer_Token *toks, isize_t start,
                           struct DiagVec *diags)
{
    struct Parser_Class *self = &node->class_;
    *self = (struct Parser_Class){};

    bool is_struct;
    isize_t l_curly = parse_class_entry(node, toks, start, &is_struct, diags);

    if (toks[l_curly].type == LEXER_TOKENTYPE_SEMICOLON) {
        return l_curly;
    } else if (toks[l_curly].type != LEXER_TOKENTYPE_L_CURLY) {
        gen_dynpush(diags, expected_token("';'", &toks[start]));
        return l_curly;
    }

    isize_t r_curly = Parser_find_twin_curly(toks, l_curly, ISIZE_MAX);
    if (r_curly == -1) {
        gen_dynpush(diags, expected_token("'}'", &toks[start]));
        r_curly = l_curly;
    }

    parse_class_body(node, toks, l_curly, r_curly, diags);
    return r_curly + 1;
}
