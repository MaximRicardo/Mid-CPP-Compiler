#include "sema/func.h"
#include "ints.h"
#include "parser/ast.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"

static bool decl_is_typedef(const struct midpar_VarDecl *decl)
{
    return decl->insts.arr[0]->type.squals.is_typedef;
}

static enum midsema_FuncConstexprSuitability
func_body_is_constexpr_suitable(const struct midpar_FuncDecl *self)
{
    if (self->quals.is_delete || self->quals.is_default)
        return MIDSEMA_FUNCCONSTEXPR_SUITABLE;

    // FIXME: add support for null statements, static_assert and using

    bool ret_found = false;
    for (mid_isize i = 0; i < self->nodes.len; ++i) {
        const struct midpar_ASTNode *node = self->nodes.arr[i];

        if (node->type == MIDPAR_ASTNODETYPE_RETURN) {
            if (midpar_func_is_ctor(self))
                return MIDSEMA_FUNCCONSTEXPR_RET_IN_CTOR;
            else if (ret_found)
                return MIDSEMA_FUNCCONSTEXPR_MULTIPLE_RET;
            ret_found = true;
        } else if (node->type == MIDPAR_ASTNODETYPE_VAR_DECL) {
            if (!decl_is_typedef(&node->var_decl))
                return MIDSEMA_FUNCCONSTEXPR_BAD_BODY;
        } else {
            return MIDSEMA_FUNCCONSTEXPR_BAD_BODY;
        }
    }

    return MIDSEMA_FUNCCONSTEXPR_SUITABLE;
}

enum midsema_FuncConstexprSuitability
midsema_func_constexpr_suitability(const struct midpar_FuncDecl *self)
{
    if (self->quals.is_virtual)
        return MIDSEMA_FUNCCONSTEXPR_VIRTUAL;

    if (!midpar_is_literal_type(&self->ret))
        return MIDSEMA_FUNCCONSTEXPR_NONLITERAL_RET;

    for (int i = 0; i < self->params.len; ++i) {
        const struct midpar_VarDeclInst *param =
            self->params.arr[i]->insts.arr[0];
        if (!midpar_is_literal_type(&param->type))
            return MIDSEMA_FUNCCONSTEXPR_NONLITERAL_PARAM;
    }

    return func_body_is_constexpr_suitable(self);
}
