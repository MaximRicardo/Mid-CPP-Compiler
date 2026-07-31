#include "ident.h"

void MidLLVM_Ident_deinit(struct MidLLVM_Ident *self)
{
    free(self->name);
}
