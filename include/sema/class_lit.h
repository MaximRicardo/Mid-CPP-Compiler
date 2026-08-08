#pragma once

#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

// can only represent structs of literal type
struct midsema_StructLit {
    struct midpar_Class *class_;
    // non-static data fields
    struct midlit_TaggedValue *dfields;
    mid_isize n_dfields;
};

void midsema_StructLit_deinit(struct midsema_StructLit *self);
// constructs according to the struct's constexpr default ctor
// returns true on success, false on failure.
// out_val        - can not be NULL
bool midsema_constexpr_default_init_struct(struct midpar_Class *struct_,
                                           struct midsema_StructLit *out_val);

// returns NULL if the field couldn't be found
struct midlit_TaggedValue *
midsema_structlit_find_field(const struct midsema_StructLit *self,
                             const char *name);

// can only represent unions of literal type
struct midsema_UnionLit {
    struct midpar_Class *class_;
    struct midlit_TaggedValue *sel_val;
    struct midpar_Type *sel_type;
};

void midsema_UnionLit_deinit(struct midsema_UnionLit *self);

#ifdef __cplusplus
}
#endif
