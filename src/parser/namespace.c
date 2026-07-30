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

void Parser_copy_namespace(struct Parser_Namespace *dest,
                           const struct Parser_Namespace *src,
                           struct Sema_Scope *dest_scope,
                           struct Parser_Allocators *allocs)
{
    *dest = *src;

    auto old_ident = Sema_add_ident_copy(
        dest_scope, Sema_deref_identptr(&src->ident), false, allocs);
    if (old_ident)
        dest->ident = Sema_create_identptr(old_ident);
    else
        dest->ident = Sema_identptr_to_last(dest_scope);

    gen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope = Sema_create_empty_scope(src->scope->type, dest_scope,
                                           PARSER_GET_NODE(dest));

    dest->childs = Parser_copy_nodepvec(&src->childs, PARSER_GET_NODE(dest),
                                        dest->scope, allocs);
}

static void add_nmspace_to_scope(struct Sema_Scope *scope,
                                 struct Parser_Namespace *self,
                                 struct DiagVec *diags)
{
    const struct Sema_Ident *old = Sema_add_ident(
        scope, &(struct Sema_Ident){.name = self->name,
                                    .decl = PARSER_GET_NODE(self),
                                    .def = PARSER_GET_NODE(self),
                                    .type = SEMA_IDENTTYPE_NAMESPACE});

    if (old)
        gen_dynpush(diags,
                    Diag_ident_redefined_err(self->name, PARSER_GET_START(self),
                                             ERRORTYPE_BAD_IDENTIFIER));
    else
        self->ident = Sema_identptr_to_last(scope);
}

//   namespace Name { ... }
//   ^              ^
// start           ret
static isize_t parse_entry(struct Parser_Namespace *self,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           struct DiagVec *diags)
{
    isize_t name_idx = start + 1;
    if (toks[name_idx].type != LEXER_TOKENTYPE_IDENTIFIER) {
        gen_dynpush(diags, Diag_expected_token_err("identifier", &toks[start],
                                                   ERRORTYPE_UNEXPECTED_TOKEN));
        return name_idx;
    }

    self->name = toks[name_idx].ident;

    add_nmspace_to_scope(scope, self, diags);
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
                        struct Parser_Allocators *allocs)
{
    gen_bumpmalloc(&allocs->scope, &self->scope);
    *self->scope = (struct Sema_Scope){.parent = parent,
                                       .node = PARSER_GET_NODE(self),
                                       .type = SEMA_SCOPETYPE_NAMESPACE};
    gen_dynpush(&parent->childs, self->scope);
}

isize_t Parser_parse_namespace(struct Parser_Namespace *self,
                               struct Sema_Scope *parent,
                               const struct Lexer_Token *toks, isize_t start,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    *self = (struct Parser_Namespace){};
    setup_scope(parent, self, allocs);

    isize_t lcurly = parse_entry(self, parent, toks, start, diags);
    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY) {
        gen_dynpush(diags, Diag_expected_token_err("'{'", &toks[start],
                                                   ERRORTYPE_MISSING_CURLY));
        return lcurly;
    }

    isize_t rcurly = find_rcurly(lcurly, toks, diags);

    for (isize_t i = lcurly + 1; i < rcurly;) {
        struct Parser_ASTNode *child = Parser_parse_node(
            toks, i, &i, PARSER_GET_NODE(self), self->scope,
            (struct Parser_ParseNodeFlags){.skip_def = false}, allocs, diags);

        gen_dynpush(&self->childs, child);
    }

    return rcurly + 1;
}
