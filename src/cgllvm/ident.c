#include "ident.h"

void CGLLVM_Ident_deinit(struct CGLLVM_Ident *self)
{
    free(self->name);
}
