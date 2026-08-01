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

void MidParser_Namespace_deinit(struct MidParser_Namespace *self)
{
    MidGen_dyndeinit(&self->childs);
}

void MidParser_copy_namespace(struct MidParser_Namespace *dest,
                              const struct MidParser_Namespace *src,
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

    MidGen_bumpmalloc(&allocs->scope, &dest->scope);
    *dest->scope = MidSema_create_empty_scope(src->scope->type, dest_scope,
                                              MIDPARSER_GET_NODE(dest));

    dest->childs = MidParser_copy_nodepvec(
        &src->childs, MIDPARSER_GET_NODE(dest), dest->scope, allocs);
}

static void add_nmspace_to_scope(struct MidSema_Scope *scope,
                                 struct MidParser_Namespace *self,
                                 struct MidDiag_DiagVec *diags)
{
    const struct MidSema_Ident *old = MidSema_add_ident(
        scope, &(struct MidSema_Ident){.name = self->name,
                                       .decl = MIDPARSER_GET_NODE(self),
                                       .def = MIDPARSER_GET_NODE(self),
                                       .type = MIDSEMA_IDENTTYPE_NAMESPACE});

    if (old)
        MidGen_dynpush(diags, MidDiag_ident_redefined_err(
                                  self->name, MIDPARSER_GET_START(self),
                                  MIDDIAG_ERR_BAD_IDENTIFIER));
    else
        self->ident = MidSema_identptr_to_last(scope);
}

//   namespace Name { ... }
//   ^              ^
// start           ret
static mid_isize parse_entry(struct MidParser_Namespace *self,
                             struct MidSema_Scope *scope,
                             const struct MidLexer_Token *toks, mid_isize start,
                             struct MidDiag_DiagVec *diags)
{
    mid_isize name_idx = start + 1;
    if (toks[name_idx].type != MIDLEXER_TOKENTYPE_IDENTIFIER) {
        MidGen_dynpush(
            diags, MidDiag_expected_token_err("identifier", &toks[start],
                                              MIDDIAG_ERR_UNEXPECTED_TOKEN));
        return name_idx;
    }

    self->name = toks[name_idx].ident;

    add_nmspace_to_scope(scope, self, diags);
    return name_idx + 1;
}

static mid_isize find_rcurly(mid_isize lcurly,
                             const struct MidLexer_Token *toks,
                             struct MidDiag_DiagVec *diags)
{
    mid_isize rcurly = MidParser_find_twin_curly(toks, lcurly, MID_ISIZE_MAX);
    if (rcurly == -1)
        MidGen_dynpush(diags,
                       MidDiag_expected_token_err("'}'", &toks[lcurly],
                                                  MIDDIAG_ERR_MISSING_CURLY));

    return rcurly == -1 ? lcurly : rcurly;
}

static void setup_scope(struct MidSema_Scope *parent,
                        struct MidParser_Namespace *self,
                        struct MidParser_Allocators *allocs)
{
    MidGen_bumpmalloc(&allocs->scope, &self->scope);
    *self->scope = (struct MidSema_Scope){.parent = parent,
                                          .node = MIDPARSER_GET_NODE(self),
                                          .type = MIDSEMA_SCOPETYPE_NAMESPACE};
    MidGen_dynpush(&parent->childs, self->scope);
}

mid_isize MidParser_parse_namespace(struct MidParser_Namespace *self,
                                    struct MidSema_Scope *parent,
                                    const struct MidLexer_Token *toks,
                                    mid_isize start,
                                    struct MidParser_Allocators *allocs,
                                    struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_Namespace){};
    setup_scope(parent, self, allocs);

    mid_isize lcurly = parse_entry(self, parent, toks, start, diags);
    if (toks[lcurly].type != MIDLEXER_TOKENTYPE_L_CURLY) {
        MidGen_dynpush(diags,
                       MidDiag_expected_token_err("'{'", &toks[start],
                                                  MIDDIAG_ERR_MISSING_CURLY));
        return lcurly;
    }

    mid_isize rcurly = find_rcurly(lcurly, toks, diags);

    for (mid_isize i = lcurly + 1; i < rcurly;) {
        struct MidParser_ASTNode *child = MidParser_parse_node(
            toks, i, &i, MIDPARSER_GET_NODE(self), self->scope,
            (struct MidParser_ParseNodeFlags){.skip_def = false}, allocs,
            diags);

        MidGen_dynpush(&self->childs, child);
    }

    return rcurly + 1;
}
