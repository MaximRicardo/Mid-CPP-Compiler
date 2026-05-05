#pragma once

#include "generics/dynarray.h"

enum Sema_IdentType {
    SEMA_IDENTTYPE_VAR,
    SEMA_IDENTTYPE_TYPEDEF,
    SEMA_IDENTTYPE_FUNC,
    SEMA_IDENTTYPE_CLASS,
    SEMA_IDENTTYPE_ENUM,
    SEMA_IDENTTYPE_NAMESPACE,
};

struct Sema_Ident {
    const char *name;
    struct Parser_ASTNode *decl;
    struct Parser_ASTNode *def;
    enum Sema_IdentType type;
};

gen_dynarray_struct_named(Sema_IdentVec, struct Sema_Ident);
