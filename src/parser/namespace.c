#include "parser/namespace.h"
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

void midpar_Namespace_deinit(struct midpar_Namespace *self)
{
    midgen_dyndeinit(&self->childs);
}

void midpar_copy_namespace(struct midpar_Namespace *dest,
                           const struct midpar_Namespace *src,
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

    midgen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope = midsema_create_empty_scope(src->scope->type, dest_scope,
                                              MIDPAR_GET_NODE(dest));

    dest->childs = midpar_copy_nodepvec(&src->childs, MIDPAR_GET_NODE(dest),
                                        dest->scope, allocs);
}

static void add_nmspace_to_scope(struct midsema_Scope *scope,
                                 struct midpar_Namespace *self,
                                 struct mid_DiagVec *diags)
{
    const struct midsema_Ident *old = midsema_add_ident(
        scope, &(struct midsema_Ident){.name = self->name,
                                       .decl = MIDPAR_GET_NODE(self),
                                       .def = MIDPAR_GET_NODE(self),
                                       .type = MIDSEMA_IDENTTYPE_NAMESPACE});

    if (old)
        midgen_dynpush(diags, middiag_ident_redefined_err(
                                  self->name, MIDPAR_GET_START(self),
                                  MIDDIAG_ERR_BAD_IDENTIFIER));
    else
        self->ident = midsema_identptr_to_last(scope);
}

//   namespace Name { ... }
//   ^              ^
// start           ret
static midlex_TokenIter parse_entry(struct midpar_Namespace *self,
                                    struct midsema_Scope *scope,
                                    midlex_TokenIter start,
                                    struct mid_DiagVec *diags)
{
    midlex_TokenIter name_idx = start + 1;
    if (name_idx->type != MIDLEX_TOKENTYPE_IDENTIFIER) {
        midgen_dynpush(
            diags, middiag_expected_token_err("identifier", start,
                                              MIDDIAG_ERR_UNEXPECTED_TOKEN));
        return name_idx;
    }

    self->name = name_idx->ident;

    add_nmspace_to_scope(scope, self, diags);
    return name_idx + 1;
}

static midlex_TokenIter find_rcurly(midlex_TokenIter lcurly,
                                    struct mid_DiagVec *diags)
{
    midlex_TokenIter rcurly = midpar_find_twin_curly(lcurly, nullptr);
    if (!rcurly)
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "'}'", lcurly, MIDDIAG_ERR_MISSING_CURLY));

    return !rcurly ? lcurly : rcurly;
}

static void setup_scope(struct midsema_Scope *parent,
                        struct midpar_Namespace *self,
                        struct midpar_Allocators *allocs)
{
    midgen_bumpmalloc(&allocs->scope, &self->scope);
    *self->scope = (struct midsema_Scope){.parent = parent,
                                          .node = MIDPAR_GET_NODE(self),
                                          .type = MIDSEMA_SCOPETYPE_NAMESPACE};
    midgen_dynpush(&parent->childs, self->scope);
}

midlex_TokenIter midpar_parse_namespace(struct midpar_Namespace *self,
                                        struct midsema_Scope *parent,
                                        midlex_TokenIter start,
                                        struct midpar_Allocators *allocs,
                                        struct mid_DiagVec *diags)
{
    *self = (struct midpar_Namespace){};
    setup_scope(parent, self, allocs);

    midlex_TokenIter lcurly = parse_entry(self, parent, start, diags);
    if (lcurly->type != MIDLEX_TOKENTYPE_L_CURLY) {
        midgen_dynpush(diags, middiag_expected_token_err(
                                  "'{'", start, MIDDIAG_ERR_MISSING_CURLY));
        return lcurly;
    }

    midlex_TokenIter rcurly = find_rcurly(lcurly, diags);

    for (midlex_TokenIter i = lcurly + 1; i < rcurly;) {
        struct midpar_ASTNode *child = midpar_parse_node(
            i, &i, MIDPAR_GET_NODE(self), self->scope,
            (struct midpar_ParseNodeFlags){.skip_def = false}, allocs, diags);

        midgen_dynpush(&self->childs, child);
    }

    return rcurly + 1;
}
