#include "sema/var.h"
#include "parser/func_decl.h"
#include "sema/type.h"

bool midsema_var_inst_is_inited(const struct midpar_VarDeclInst *self)
{
    if (self->has_ctor || self->init.expr)
        return true;

    return midsema_type_is_default_constructible(&self->type);
}

bool midsema_var_inst_is_constexpr_inited(const struct midpar_VarDeclInst *self)
{
    if (self->has_ctor)
        return self->ctor.ctor->quals.is_constexpr;

    if (self->init.expr)
        return self->init.expr->constant;

    return midsema_type_is_constexpr_default_constructible(&self->type);
}
