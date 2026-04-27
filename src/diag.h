#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "position.h"

enum ErrorType {
    ERRORTYPE_UNKNOWN_SYMBOL,
    ERRORTYPE_MISSING_PAREN,
    ERRORTYPE_INSUFFICIENT_OPERANDS,
};

enum WarnType {
    WARNTYPE_SHUT_COMPILER_UP,
};

struct Diag {
    struct Position pos;
    const char *line; // terminated by '\n'
    char *msg;
    union {
        enum ErrorType err;
        enum WarnType warn;
    };
    bool is_err;
};
gen_dynarray_struct_named(DiagVec, struct Diag);

void Diag_deinit(struct Diag *self);
void Diag_print(const struct Diag *diag);
