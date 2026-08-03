#include "cgllvm/ident.h"

void midllvm_Ident_deinit(struct midllvm_Ident *self)
{
    free(self->name);
}
