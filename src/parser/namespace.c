#include "namespace.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/find_twin.h"
#include "sema/ident.h"
#include "sema/scope.h"

void Parser_Namespace_deinit(struct Parser_Namespace *self)
{
    gen_dyndeinit(&self->childs);
}

void Parser_copy_namespace(struct Parser_ASTNode *dest_node,
                           const struct Parser_ASTNode *src_node,
                           struct Sema_Scope *dest_scope,
                           struct Parser_Allocators *allocs)
{
    auto dest = &dest_node->nmspace;
    auto src = &src_node->nmspace;

    *dest = *src;

    auto old_ident = Sema_add_ident_copy(
        dest_scope, Parser_namespace_ident(src), false, allocs);
    if (old_ident)
        dest->ident_idx = old_ident - dest_scope->idents.arr;
    else
        dest->ident_idx = dest_scope->idents.len - 1;

    gen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope =
        Sema_create_empty_scope(src->scope->type, dest_scope, dest_node);

    dest->childs =
        Parser_copy_nodepvec(&src->childs, dest_node, dest->scope, allocs);
}

struct Sema_Ident *Parser_namespace_ident(const struct Parser_Namespace *self)
{
    assert(self->ident_idx != -1);
    return &self->scope->parent->idents.arr[self->ident_idx];
}

static void add_nmspace_to_scope(struct Sema_Scope *scope,
                                 struct Parser_ASTNode *node,
                                 struct DiagVec *diags)
{
    auto self = &node->nmspace;

    const struct Sema_Ident *old = Sema_add_ident(
        scope, &(struct Sema_Ident){.name = self->name,
                                    .decl = node,
                                    .def = node,
                                    .type = SEMA_IDENTTYPE_NAMESPACE});

    if (old)
        gen_dynpush(diags, Diag_ident_redefined_err(self->name, node->start,
                                                    ERRORTYPE_BAD_IDENTIFIER));
    else
        self->ident_idx = scope->idents.len - 1;
}

//   namespace Name { ... }
//   ^              ^
// start           ret
static isize_t parse_entry(struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           struct DiagVec *diags)
{
    auto self = &node->nmspace;

    isize_t name_idx = start + 1;
    if (toks[name_idx].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, Diag_expected_token_err("identifier", &toks[start],
                                                   ERRORTYPE_UNEXPECTED_TOKEN));
        return name_idx;
    }

    self->name = toks[name_idx].ident;

    add_nmspace_to_scope(scope, node, diags);
    return name_idx + 1;
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

static void setup_scope(struct Sema_Scope *parent,
                        struct Parser_Namespace *self,
                        struct Parser_ASTNode *node,
                        struct Parser_Allocators *allocs)
{
    gen_bumpmalloc(&allocs->scope, &self->scope);
    *self->scope = (struct Sema_Scope){
        .parent = parent, .node = node, .type = SEMA_SCOPETYPE_NAMESPACE};
    gen_dynpush(&parent->childs, self->scope);
}

isize_t Parser_parse_namespace(struct Parser_ASTNode *node,
                               struct Sema_Scope *parent,
                               const struct Lexer_Token *toks, isize_t start,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    auto self = &node->nmspace;
    *self = (struct Parser_Namespace){.ident_idx = -1};
    setup_scope(parent, self, node, allocs);

    isize_t lcurly = parse_entry(node, parent, toks, start, diags);
    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY) {
        gen_dynpush(diags, Diag_expected_token_err("'{'", &toks[start],
                                                   ERRORTYPE_MISSING_CURLY));
        return lcurly;
    }

    isize_t rcurly = find_rcurly(lcurly, toks, diags);

    for (isize_t i = lcurly + 1; i < rcurly;) {
        struct Parser_ASTNode *child = Parser_parse_node(
            toks, i, &i, node, self->scope,
            (struct Parser_ParseNodeFlags){.skip_def = false}, allocs, diags);

        gen_dynpush(&self->childs, child);
    }

    return rcurly + 1;
}
