#include "scope.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/func_decl.h"
#include "parser/template.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/type.h"
#include <string.h>

void MidSema_Scope_deinit(struct MidSema_Scope *self)
{
    MidGen_dyndeinit(&self->childs);
    MidGen_dyndeinit(&self->idents, MidSema_Ident_deinit);
}

void MidSema_copy_scope(struct MidSema_Scope *dest, const struct MidSema_Scope *src,
                     struct MidSema_Scope *dest_parent,
                     struct MidParser_Allocators *allocs)
{
    *dest = *src;

    dest->parent = dest_parent;
    dest->idents = (struct MidSema_IdentVec){};
    MidGen_dynreserve(&dest->idents, src->idents.len);
    dest->childs = (struct MidSema_ScopePVec){};
    MidGen_dynreserve(&dest->childs, src->childs.len);

    for (mid_isize i = 0; i < src->idents.len; ++i) {
        MidGen_dynpush(&dest->idents,
                    MidSema_copy_ident(&src->idents.arr[i], dest, true, allocs));
    }

    for (mid_isize i = 0; i < src->childs.len; ++i) {
        struct MidSema_Scope *cpy_child;
        MidGen_bumpmalloc(&allocs->scope, &cpy_child);

        MidSema_copy_scope(cpy_child, src->childs.arr[i], dest, allocs);

        MidGen_dynpush(&dest->childs, cpy_child);
    }
}

struct MidSema_Scope MidSema_create_empty_scope(enum MidSema_ScopeType type,
                                          struct MidSema_Scope *parent,
                                          struct MidParser_ASTNode *node)
{
    return (struct MidSema_Scope){
        .type = type,
        .parent = parent,
        .node = node,
    };
}

bool MidSema_is_rnce_scope(enum MidSema_ScopeType type)
{
    return type == MIDSEMA_SCOPETYPE_ROOT || type == MIDSEMA_SCOPETYPE_NAMESPACE ||
           type == MIDSEMA_SCOPETYPE_CLASS || type == MIDSEMA_SCOPETYPE_ENUM;
}

const struct MidSema_Scope *
MidSema_closest_rnce_scope_const(const struct MidSema_Scope *self)
{
    if (MidSema_is_rnce_scope(self->type))
        return self;
    else if (self->parent)
        return MidSema_closest_rnce_scope_const(self->parent);
    else
        return NULL;
}

struct MidSema_Scope *MidSema_closest_rnce_scope(struct MidSema_Scope *self)
{
    return (struct MidSema_Scope *)MidSema_closest_rnce_scope_const(self);
}

const struct MidSema_Scope *
MidSema_closest_scope_of_type_const(const struct MidSema_Scope *self,
                                 enum MidSema_ScopeType type)
{
    if (self->type == type)
        return self;
    else if (self->parent)
        return MidSema_closest_scope_of_type_const(self->parent, type);
    else
        return NULL;
}

struct MidSema_Scope *MidSema_closest_scope_of_type(struct MidSema_Scope *self,
                                              enum MidSema_ScopeType type)
{
    return (struct MidSema_Scope *)MidSema_closest_scope_of_type_const(self, type);
}

static const struct MidSema_Ident *
find_ident_in_arr(const char *name, const struct MidSema_Ident *idents, mid_isize n)
{
    for (mid_isize i = 0; i < n; ++i) {
        auto ident = &idents[i];
        if (!strcmp(ident->name, name)) {
            return ident;
        }
    }

    return NULL;
}

static const struct MidSema_Ident *
search_child_tmplt_scopes(const char *name, const struct MidSema_Scope *scope)
{
    for (mid_isize i = 0; i < scope->childs.len; ++i) {
        auto child = scope->childs.arr[i];
        if (child->type != MIDSEMA_SCOPETYPE_TEMPLATE)
            continue;

        const struct MidSema_Ident *ident =
            MidParser_tmplt_ident(&child->node->tmplt);
        if (!strcmp(ident->name, name))
            return ident;
    }

    return NULL;
}

const struct MidSema_Ident *MidSema_find_ident_const(const struct MidSema_Scope *scope,
                                               const char *name)
{
    const struct MidSema_Ident *ident = NULL;
    if (!ident)
        ident = find_ident_in_arr(name, scope->idents.arr, scope->idents.len);
    if (!ident)
        ident = search_child_tmplt_scopes(name, scope);

    if (ident)
        return ident;

    if (scope->parent)
        return MidSema_find_ident_const(scope->parent, name);
    return NULL;
}

struct MidSema_Ident *MidSema_find_ident(struct MidSema_Scope *scope, const char *name)
{
    return (struct MidSema_Ident *)MidSema_find_ident_const(scope, name);
}

bool MidSema_is_type_name(const struct MidSema_Scope *scope, const char *name)
{
    auto ident = MidSema_find_ident_const(scope, name);
    if (!ident)
        return false;

    if (ident->decl && MidSema_node_creates_type_name(ident->decl))
        return true;
    return false;
}

bool MidSema_is_namespace_name(const struct MidSema_Scope *scope, const char *name)
{
    auto ident = MidSema_find_ident_const(scope, name);
    if (!ident)
        return false;

    return ident->type == MIDSEMA_IDENTTYPE_NAMESPACE;
}

bool MidSema_ident_type(struct MidSema_Scope *scope, const struct MidSema_Ident *ident,
                     struct MidParser_Type *out_type)
{
    if (ident->type == MIDSEMA_IDENTTYPE_NAMESPACE || !ident->decl) {
        return false;
    } else {
        if (out_type)
            *out_type = MidSema_node_type(ident->decl, scope);
        return true;
    }
}

bool MidSema_name_type(struct MidSema_Scope *scope, const char *name,
                    struct MidParser_Type *out_type)
{
    auto ident = MidSema_find_ident_const(scope, name);
    if (!ident)
        return false;

    return MidSema_ident_type(scope, ident, out_type);
}

struct MidParser_Type MidSema_type_name_type(struct MidSema_Scope *scope,
                                       const char *name)
{
    auto ident = MidSema_find_ident(scope, name);
    if (!ident->decl)
        MID_CRASH("identifier doesn't exist");

    switch (ident->type) {
    case MIDSEMA_IDENTTYPE_CLASS: {
        auto type = MidParser_toktype_to_type(ident->decl->class_.type ==
                                                   MIDPARSER_CLASSTYPE_UNION
                                               ? MIDLEXER_TOKENTYPE_UNION
                                               : MIDLEXER_TOKENTYPE_CLASS);
        type.named = MidSema_create_identptr(ident);
        return type;
    }

    case MIDSEMA_IDENTTYPE_ENUM: {
        auto type = MidParser_toktype_to_type(MIDLEXER_TOKENTYPE_ENUM);
        type.named = MidSema_create_identptr(ident);
        return type;
    }

    case MIDSEMA_IDENTTYPE_TYPEDEF:
        if (ident->decl->type == MIDPARSER_ASTNODETYPE_VAR_DECL_INST) {
            auto inst = &ident->decl->var_inst;
            return MidParser_copy_type(&inst->type);
        } else if (ident->decl->type == MIDPARSER_ASTNODETYPE_TMPLT_PARAM) {
            assert(ident->decl->tmplt_param.kind == MIDPARSER_TMPLTPARAM_TYPE);
            auto tmplt = &ident->decl->parent->tmplt;
            auto scope = tmplt->scope;
            auto param = &ident->decl->tmplt_param.type;
            return MidParser_create_templated_type((struct MidSema_IdentPtr){
                .parent = scope, .idx = param->ident_idx});
        }
        MID_CRASH("name not typedefed in node");

    default:
        break;
    }

    MID_CRASH("identifier doesn't declare a type");
}

struct MidParser_Type MidSema_tok_type(struct MidSema_Scope *scope,
                                 const struct MidLexer_Token *tok)
{
    if (tok->type == MIDLEXER_TOKENTYPE_IDENTIFIER)
        return MidSema_type_name_type(scope, tok->ident);
    else
        return MidParser_toktype_to_type(tok->type);
}

static bool are_params_same(const struct MidParser_FuncDecl *a,
                            const struct MidParser_FuncDecl *b)
{
    if (a->params.len != b->params.len || a->variadic != b->variadic)
        return false;

    for (mid_isize i = 0; i < a->params.len; ++i) {
        if (!MidParser_are_types_same(&a->params.arr[i]->insts.arr[0]->type,
                                   &b->params.arr[i]->insts.arr[0]->type))
            return false;
    }

    return true;
}

static bool are_func_decls_same(const struct MidParser_FuncDecl *a,
                                const struct MidParser_FuncDecl *b)
{
    if (a->is_op_overload != b->is_op_overload)
        return false;
    else if (a->is_op_overload && a->op_overload != b->op_overload)
        return false;
    else if (!a->is_op_overload && strcmp(a->name, b->name))
        return false;
    else if (memcmp(&a->quals, &b->quals, sizeof(a->quals)))
        return false;

    return are_params_same(a, b);
}

static bool are_idents_equiv(const struct MidSema_Ident *a,
                             const struct MidSema_Ident *b)
{
    if (strcmp(a->name, b->name))
        return false;

    if (a->type == MIDSEMA_IDENTTYPE_FUNC && b->type == MIDSEMA_IDENTTYPE_FUNC &&
        !are_func_decls_same(&a->decl->func_decl, &b->decl->func_decl))
        return false;

    return true;
}

static struct MidSema_Ident *
find_ident_in_child_tmplt_scopes(const struct MidSema_Scope *scope,
                                 const struct MidSema_Ident *search)
{
    for (mid_isize i = 0; i < scope->childs.len; ++i) {
        auto child = scope->childs.arr[i];
        if (child->type != MIDSEMA_SCOPETYPE_TEMPLATE)
            continue;

        auto ident = MidParser_tmplt_ident(&child->node->tmplt);
        if (are_idents_equiv(ident, search))
            return ident;
    }

    return NULL;
}

// finds an equivalent identifier that could cause a name collision
// only checks the scope itself and not its parents
static struct MidSema_Ident *find_ident_shallow(const struct MidSema_Scope *scope,
                                             const struct MidSema_Ident *search)
{
    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (are_idents_equiv(ident, search))
            return ident;
    }

    auto tmplt_ident = find_ident_in_child_tmplt_scopes(scope, search);
    if (tmplt_ident)
        return tmplt_ident;

    return NULL;
}

struct MidSema_Ident *MidSema_add_ident(struct MidSema_Scope *scope,
                                  struct MidSema_Ident *ident)
{
    auto old_ident = find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    ident->parent = scope;
    MidGen_dynpush(&scope->idents, *ident);
    return NULL;
}

struct MidSema_Ident *MidSema_add_ident_copy(struct MidSema_Scope *scope,
                                       const struct MidSema_Ident *ident,
                                       bool copy_ident_scopes,
                                       struct MidParser_Allocators *allocs)
{
    auto old_ident = find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    auto cpy = MidSema_copy_ident(ident, scope, copy_ident_scopes, allocs);
    cpy.parent = scope;
    MidGen_dynpush(&scope->idents, cpy);
    return NULL;
}

i32 MidSema_add_ident_def(struct MidSema_Scope *scope, const char *name,
                       struct MidParser_ASTNode *def)
{
    auto ident = MidSema_find_ident(scope, name);
    assert(ident);

    if (ident->def)
        return 1;

    ident->def = def;
    return 0;
}

const struct MidSema_Scope *
MidSema_resolve_scope_const(const char *name, const struct MidSema_Scope *scope)
{
    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (!MidSema_is_nce_ident(ident->type))
            continue;
        if (strcmp(ident->name, name))
            continue;

        return MidSema_ident_scope(ident);
    }

    if (!scope->parent)
        return NULL;

    // keep searching through the parent scopes
    auto next = MidSema_closest_rnce_scope_const(scope->parent);
    return MidSema_resolve_scope_const(name, next);
}

struct MidSema_Scope *MidSema_resolve_scope(const char *name,
                                      struct MidSema_Scope *scope)
{
    return (struct MidSema_Scope *)MidSema_resolve_scope_const(name, scope);
}

const char *MidSema_scope_name(const struct MidSema_Scope *scope)
{
    const char *name =
        scope->type == MIDSEMA_SCOPETYPE_CLASS       ? scope->node->class_.name
        : scope->type == MIDSEMA_SCOPETYPE_ENUM      ? scope->node->enum_.name
        : scope->type == MIDSEMA_SCOPETYPE_NAMESPACE ? scope->node->nmspace.name
                                                  : NULL;

    return name;
}

void MidSema_add_tmplt_params_to_scope(struct MidSema_Scope *scope,
                                    const struct MidSema_Scope *tmplt)
{
    for (mid_isize i = 0; i < tmplt->idents.len; ++i) {
        // NOTE: doing a basic data copy should be safe (i hope)
        MidGen_dynpush(&scope->idents, tmplt->idents.arr[i]);
    }
}
