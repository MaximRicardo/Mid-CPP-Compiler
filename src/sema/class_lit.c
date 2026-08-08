#include "sema/class_lit.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "literal.h"
#include "mid_alloc.h"
#include "parser/ast.h"
#include "parser/var_decl.h"
#include "sema/class.h"

void midsema_StructLit_deinit(struct midsema_StructLit *self)
{
    for (mid_isize i = 0; i < self->n_dfields; ++i) {
        midlit_TaggedValue_deinit(&self->dfields[i]);
    }
    free(self->dfields);
}

bool midsema_constexpr_default_init_struct(struct midpar_Class *struct_,
                                           struct midsema_StructLit *out_val)
{
    struct midpar_VarDeclInstPVec dfields = midsema_nonstatic_dfields(struct_);

    out_val->class_ = struct_;
    out_val->n_dfields = dfields.len;
    out_val->dfields =
        mid_malloc(out_val->n_dfields * sizeof(*out_val->dfields));

    bool failed = false;
    for (mid_isize i = 0; i < dfields.len; ++i) {
        if (!midsema_field_default_value(struct_, dfields.arr[i]->name,
                                         &out_val->dfields[i])) {
            failed = true;
            break;
        }

        printf("field '%s' = ", dfields.arr[i]->name);
        midlit_tagged_print(&out_val->dfields[i]);
        printf("\n");
    }

    if (failed)
        free(out_val->dfields);
    midgen_dyndeinit(&dfields);
    return !failed;
}

struct midlit_TaggedValue *
midsema_structlit_find_field(const struct midsema_StructLit *self,
                             const char *name)
{
    struct midpar_VarDeclInstPVec dfields =
        midsema_nonstatic_dfields(self->class_);
    const struct midpar_ASTNode *field = midsema_find_field(self->class_, name);
    if (!field)
        return nullptr;

    assert(field->type == MIDPAR_ASTNODETYPE_VAR_DECL_INST);

    mid_isize i;
    for (i = 0; i < dfields.len; ++i) {
        if (field == MIDPAR_GET_NODE(dfields.arr[i]))
            break;
    }

    if (i == dfields.len)
        return nullptr;

    return &self->dfields[i];
}

void midsema_UnionLit_deinit(struct midsema_UnionLit *self)
{
    midlit_TaggedValue_deinit(self->sel_val);
    free(self->sel_val);
}
