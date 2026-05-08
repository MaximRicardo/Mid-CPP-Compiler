#include "ident.h"

void Sema_IdentFuncInfo_deinit(struct Sema_IdentFuncInfo *self)
{
    free(self->default_args);
}

void Sema_Ident_deinit(struct Sema_Ident *self)
{
    if (self->type == SEMA_IDENTTYPE_FUNC)
        Sema_IdentFuncInfo_deinit(&self->func_info);
}
