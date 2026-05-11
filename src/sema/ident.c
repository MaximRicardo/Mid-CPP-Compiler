#include "ident.h"
#include "macros.h"

bool Sema_is_nce_ident(enum Sema_IdentType type)
{
    return type == SEMA_IDENTTYPE_NAMESPACE || type == SEMA_IDENTTYPE_CLASS ||
           type == SEMA_IDENTTYPE_ENUM;
}

void Sema_IdentFuncInfo_deinit(struct Sema_IdentFuncInfo *self)
{
    free(self->default_args);
}

void Sema_Ident_deinit(struct Sema_Ident *self)
{
    if (self->type == SEMA_IDENTTYPE_FUNC)
        Sema_IdentFuncInfo_deinit(&self->func_info);
}

struct Sema_Scope *Sema_ident_scope(const struct Sema_Ident *self)
{
    switch (self->type) {
    case SEMA_IDENTTYPE_FUNC:
        return self->func_info.def_scope;

    case SEMA_IDENTTYPE_CLASS:
        return self->class_info.def_scope;

    case SEMA_IDENTTYPE_ENUM:
        CRASH("scope of enum ident not implemented yet");

    case SEMA_IDENTTYPE_NAMESPACE:
        CRASH("scope of namespace ident not implemented yet");

    default:
        CRASH("ident doesn't have it's own scope");
    }
}
