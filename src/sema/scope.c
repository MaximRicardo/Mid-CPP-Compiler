#include "sema/scope.h"
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
#include "sema/typecheck.h"
#include <string.h>

void midsema_Scope_deinit(struct midsema_Scope *self)
{
    midgen_dyndeinit(&self->childs);
    midgen_dyndeinit(&self->idents, midsema_Ident_deinit);
}

void midsema_copy_scope(struct midsema_Scope *dest,
                        const struct midsema_Scope *src,
                        struct midsema_Scope *dest_parent,
                        struct midpar_Allocators *allocs)
{
    *dest = *src;

    dest->parent = dest_parent;
    dest->idents = (struct midsema_IdentVec){};
    midgen_dynreserve(&dest->idents, src->idents.len);
    dest->childs = (struct midsema_ScopePVec){};
    midgen_dynreserve(&dest->childs, src->childs.len);

    for (mid_isize i = 0; i < src->idents.len; ++i) {
        midgen_dynpush(&dest->idents, midsema_copy_ident(&src->idents.arr[i],
                                                         dest, true, allocs));
    }

    for (mid_isize i = 0; i < src->childs.len; ++i) {
        struct midsema_Scope *cpy_child;
        midgen_bumpmalloc(&allocs->scope, &cpy_child);

        midsema_copy_scope(cpy_child, src->childs.arr[i], dest, allocs);

        midgen_dynpush(&dest->childs, cpy_child);
    }
}

struct midsema_Scope midsema_create_empty_scope(enum midsema_ScopeType type,
                                                struct midsema_Scope *parent,
                                                struct midpar_ASTNode *node)
{
    return (struct midsema_Scope){
        .type = type,
        .parent = parent,
        .node = node,
    };
}

bool midsema_is_rnce_scope(enum midsema_ScopeType type)
{
    return type == MIDSEMA_SCOPETYPE_ROOT ||
           type == MIDSEMA_SCOPETYPE_NAMESPACE ||
           type == MIDSEMA_SCOPETYPE_CLASS || type == MIDSEMA_SCOPETYPE_ENUM;
}

const struct midsema_Scope *
midsema_closest_rnce_scope_const(const struct midsema_Scope *self)
{
    if (midsema_is_rnce_scope(self->type))
        return self;
    else if (self->parent)
        return midsema_closest_rnce_scope_const(self->parent);
    else
        return NULL;
}

struct midsema_Scope *midsema_closest_rnce_scope(struct midsema_Scope *self)
{
    return (struct midsema_Scope *)midsema_closest_rnce_scope_const(self);
}

const struct midsema_Scope *
midsema_closest_scope_of_type_const(const struct midsema_Scope *self,
                                    enum midsema_ScopeType type)
{
    if (self->type == type)
        return self;
    else if (self->parent)
        return midsema_closest_scope_of_type_const(self->parent, type);
    else
        return NULL;
}

struct midsema_Scope *midsema_closest_scope_of_type(struct midsema_Scope *self,
                                                    enum midsema_ScopeType type)
{
    return (struct midsema_Scope *)midsema_closest_scope_of_type_const(self,
                                                                       type);
}

static const struct midsema_Ident *
find_ident_in_arr(const char *name, const struct midsema_Ident *idents,
                  mid_isize n)
{
    for (mid_isize i = 0; i < n; ++i) {
        auto ident = &idents[i];
        if (!strcmp(ident->name, name)) {
            return ident;
        }
    }

    return NULL;
}

static const struct midsema_Ident *
search_child_tmplt_scopes(const char *name, const struct midsema_Scope *scope)
{
    for (mid_isize i = 0; i < scope->childs.len; ++i) {
        auto child = scope->childs.arr[i];
        if (child->type != MIDSEMA_SCOPETYPE_TEMPLATE)
            continue;

        const struct midsema_Ident *ident =
            midpar_tmplt_ident(&child->node->tmplt);
        if (!strcmp(ident->name, name))
            return ident;
    }

    return NULL;
}

const struct midsema_Ident *
midsema_find_ident_const(const struct midsema_Scope *scope, const char *name)
{
    const struct midsema_Ident *ident = NULL;
    if (!ident)
        ident = find_ident_in_arr(name, scope->idents.arr, scope->idents.len);
    if (!ident)
        ident = search_child_tmplt_scopes(name, scope);

    if (ident)
        return ident;

    if (scope->parent)
        return midsema_find_ident_const(scope->parent, name);
    return NULL;
}

struct midsema_Ident *midsema_find_ident(struct midsema_Scope *scope,
                                         const char *name)
{
    return (struct midsema_Ident *)midsema_find_ident_const(scope, name);
}

bool midsema_is_type_name(const struct midsema_Scope *scope, const char *name)
{
    auto ident = midsema_find_ident_const(scope, name);
    if (!ident)
        return false;

    if (ident->decl && midsema_node_creates_new_type(ident->decl))
        return true;
    return false;
}

bool midsema_is_namespace_name(const struct midsema_Scope *scope,
                               const char *name)
{
    auto ident = midsema_find_ident_const(scope, name);
    if (!ident)
        return false;

    return ident->type == MIDSEMA_IDENTTYPE_NAMESPACE;
}

bool midsema_ident_type(struct midsema_Scope *scope,
                        const struct midsema_Ident *ident,
                        struct midpar_Type *out_type)
{
    if (ident->type == MIDSEMA_IDENTTYPE_NAMESPACE || !ident->decl) {
        return false;
    } else {
        if (out_type)
            *out_type = midsema_node_type(ident->decl, scope);
        return true;
    }
}

bool midsema_name_type(struct midsema_Scope *scope, const char *name,
                       struct midpar_Type *out_type)
{
    auto ident = midsema_find_ident_const(scope, name);
    if (!ident)
        return false;

    return midsema_ident_type(scope, ident, out_type);
}

struct midpar_Type midsema_type_name_type(struct midsema_Scope *scope,
                                          const char *name)
{
    auto ident = midsema_find_ident(scope, name);
    if (!ident->decl)
        MID_CRASH("identifier doesn't exist");

    switch (ident->type) {
    case MIDSEMA_IDENTTYPE_CLASS: {
        auto type = midpar_toktype_to_type(ident->decl->class_.type ==
                                                   MIDPAR_CLASSTYPE_UNION
                                               ? MIDLEX_TOKENTYPE_UNION
                                               : MIDLEX_TOKENTYPE_CLASS);
        type.named = midsema_create_identptr(ident);
        return type;
    }

    case MIDSEMA_IDENTTYPE_ENUM: {
        auto type = midpar_toktype_to_type(MIDLEX_TOKENTYPE_ENUM);
        type.named = midsema_create_identptr(ident);
        return type;
    }

    case MIDSEMA_IDENTTYPE_TYPEDEF:
        if (ident->decl->type == MIDPAR_ASTNODETYPE_VAR_DECL_INST) {
            auto inst = &ident->decl->var_inst;
            return midpar_copy_type(&inst->type);
        } else if (ident->decl->type == MIDPAR_ASTNODETYPE_TMPLT_PARAM) {
            assert(ident->decl->tmplt_param.kind == MIDPAR_TMPLTPARAM_TYPE);
            auto tmplt = &ident->decl->parent->tmplt;
            auto scope = tmplt->scope;
            auto param = &ident->decl->tmplt_param.type;
            return midpar_create_templated_type((struct midsema_IdentPtr){
                .parent = scope, .idx = param->ident_idx});
        }
        MID_CRASH("name not typedefed in node");

    default:
        break;
    }

    MID_CRASH("identifier doesn't declare a type");
}

struct midpar_Type midsema_tok_type(struct midsema_Scope *scope,
                                    const struct midlex_Token *tok)
{
    if (tok->type == MIDLEX_TOKENTYPE_IDENTIFIER)
        return midsema_type_name_type(scope, tok->ident);
    else
        return midpar_toktype_to_type(tok->type);
}

static bool are_params_same(const struct midpar_FuncDecl *a,
                            const struct midpar_FuncDecl *b)
{
    if (a->params.len != b->params.len || a->variadic != b->variadic)
        return false;

    for (mid_isize i = 0; i < a->params.len; ++i) {
        if (!midsema_are_types_same(&a->params.arr[i]->insts.arr[0]->type,
                                    &b->params.arr[i]->insts.arr[0]->type))
            return false;
    }

    return true;
}

static bool are_func_decls_same(const struct midpar_FuncDecl *a,
                                const struct midpar_FuncDecl *b)
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

static bool are_idents_equiv(const struct midsema_Ident *a,
                             const struct midsema_Ident *b)
{
    if (strcmp(a->name, b->name))
        return false;

    if (a->type == MIDSEMA_IDENTTYPE_FUNC &&
        b->type == MIDSEMA_IDENTTYPE_FUNC &&
        !are_func_decls_same(&a->decl->func_decl, &b->decl->func_decl))
        return false;

    return true;
}

static struct midsema_Ident *
find_ident_in_child_tmplt_scopes(const struct midsema_Scope *scope,
                                 const struct midsema_Ident *search)
{
    for (mid_isize i = 0; i < scope->childs.len; ++i) {
        auto child = scope->childs.arr[i];
        if (child->type != MIDSEMA_SCOPETYPE_TEMPLATE)
            continue;

        auto ident = midpar_tmplt_ident(&child->node->tmplt);
        if (are_idents_equiv(ident, search))
            return ident;
    }

    return NULL;
}

// finds an equivalent identifier that could cause a name collision
// only checks the scope itself and not its parents
static struct midsema_Ident *
find_ident_shallow(const struct midsema_Scope *scope,
                   const struct midsema_Ident *search)
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

struct midsema_Ident *midsema_add_ident(struct midsema_Scope *scope,
                                        struct midsema_Ident *ident)
{
    auto old_ident = find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    ident->parent = scope;
    midgen_dynpush(&scope->idents, *ident);
    return NULL;
}

struct midsema_Ident *midsema_add_ident_copy(struct midsema_Scope *scope,
                                             const struct midsema_Ident *ident,
                                             bool copy_ident_scopes,
                                             struct midpar_Allocators *allocs)
{
    auto old_ident = find_ident_shallow(scope, ident);
    if (old_ident)
        return old_ident;

    auto cpy = midsema_copy_ident(ident, scope, copy_ident_scopes, allocs);
    cpy.parent = scope;
    midgen_dynpush(&scope->idents, cpy);
    return NULL;
}

int32_t midsema_add_ident_def(struct midsema_Scope *scope, const char *name,
                              struct midpar_ASTNode *def)
{
    auto ident = midsema_find_ident(scope, name);
    assert(ident);

    if (ident->def)
        return 1;

    ident->def = def;
    return 0;
}

const struct midsema_Scope *
midsema_resolve_scope_const(const char *name, const struct midsema_Scope *scope)
{
    for (mid_isize i = 0; i < scope->idents.len; ++i) {
        auto ident = &scope->idents.arr[i];
        if (!midsema_is_nce_ident(ident->type))
            continue;
        if (strcmp(ident->name, name))
            continue;

        return midsema_ident_scope(ident);
    }

    if (!scope->parent)
        return NULL;

    // keep searching through the parent scopes
    auto next = midsema_closest_rnce_scope_const(scope->parent);
    return midsema_resolve_scope_const(name, next);
}

struct midsema_Scope *midsema_resolve_scope(const char *name,
                                            struct midsema_Scope *scope)
{
    return (struct midsema_Scope *)midsema_resolve_scope_const(name, scope);
}

const char *midsema_scope_name(const struct midsema_Scope *scope)
{
    const char *name =
        scope->type == MIDSEMA_SCOPETYPE_CLASS       ? scope->node->class_.name
        : scope->type == MIDSEMA_SCOPETYPE_ENUM      ? scope->node->enum_.name
        : scope->type == MIDSEMA_SCOPETYPE_NAMESPACE ? scope->node->nmspace.name
                                                     : NULL;

    return name;
}

void midsema_add_tmplt_params_to_scope(struct midsema_Scope *scope,
                                       const struct midsema_Scope *tmplt)
{
    for (mid_isize i = 0; i < tmplt->idents.len; ++i) {
        // NOTE: doing a basic data copy should be safe (i hope)
        midgen_dynpush(&scope->idents, tmplt->idents.arr[i]);
    }
}
