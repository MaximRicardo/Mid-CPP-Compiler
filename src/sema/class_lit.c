#include "sema/class_lit.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "literal.h"
#include "mid_alloc.h"
#include "parser/ast.h"
#include "parser/var_decl.h"
#include "sema/class.h"
#include <string.h>

void midsema_StructLit_deinit(struct midsema_StructLit *self)
{
    for (mid_isize i = 0; i < self->n_dfields; ++i)
        midlit_TaggedValue_deinit(&self->dfields[i]);
    free(self->dfields);
}

struct midsema_StructLit
midsema_copy_structlit(const struct midsema_StructLit *src)
{
    struct midsema_StructLit dst = *src;
    if (src->n_dfields == 0)
        return dst;

    dst.dfields = mid_malloc(src->n_dfields * sizeof(*dst.dfields));

    for (mid_isize i = 0; i < src->n_dfields; ++i) {
        dst.dfields[i] = midlit_copy_value(&src->dfields[i]);
    }

    return dst;
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
    }

    if (failed)
        free(out_val->dfields);
    midgen_dyndeinit(&dfields);
    return !failed;
}

struct midlit_TaggedValue *
midsema_get_structlit_field(const struct midsema_StructLit *self,
                            const char *field)
{
    struct midpar_VarDeclInstPVec dfields =
        midsema_nonstatic_dfields(self->class_);

    mid_isize i;
    for (i = 0; i < dfields.len; ++i) {
        if (!strcmp(dfields.arr[i]->name, field))
            break;
    }

    midgen_dyndeinit(&dfields);
    if (i < self->n_dfields)
        return &self->dfields[i];
    else
        return nullptr;
}

const char *midsema_structlit_field_name(const struct midsema_StructLit *self,
                                         mid_isize field)
{
    struct midpar_VarDeclInstPVec dfields =
        midsema_nonstatic_dfields(self->class_);
    assert(field >= 0 && field < dfields.len);

    const char *name = dfields.arr[field]->name;

    midgen_dyndeinit(&dfields);
    return name;
}

static void print_indent(FILE *out, int indent)
{
    for (int i = 0; i < indent; ++i)
        fputc(' ', out);
}

void midsema_fprint_struclit_w_indent(FILE *out,
                                      const struct midsema_StructLit *self,
                                      int indent)
{
    struct midpar_VarDeclInstPVec dfields =
        midsema_nonstatic_dfields(self->class_);

    print_indent(out, indent);
    fprintf(out, "struct '%s' {\n",
            self->class_->name ? self->class_->name : "(anonymous)");

    for (mid_isize i = 0; i < dfields.len; ++i) {
        print_indent(out, indent + 4);
        fprintf(out, "'%s' = ", dfields.arr[i]->name);
        if (self->dfields[i].kind == MIDLIT_VALUE_STRUCT)
            midsema_fprint_struclit_w_indent(out, &self->dfields[i].v.struct_,
                                             indent + 4);
        else
            midlit_tagged_fprint(out, &self->dfields[i]);
        fprintf(out, "\n");
    }

    print_indent(out, indent);
    fprintf(out, "}");

    midgen_dyndeinit(&dfields);
}

void midsema_fprint_structlit(FILE *out, const struct midsema_StructLit *self)
{
    midsema_fprint_struclit_w_indent(out, self, 0);
}

void midsema_print_structlit(const struct midsema_StructLit *self)
{
    midsema_fprint_structlit(stdout, self);
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
