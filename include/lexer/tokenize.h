#pragma once

#include "diag.h"
#include "literal.h"
#include "symbol.h"
#include "token.h"

#ifdef __cplusplus
extern "C" {
#endif

struct midlex_Tokenize {
    struct midlex_TokenVec toks;
    struct midsymb_Table symtbl;
    struct midlit_StringVec str_lits;
    struct mid_DiagVec diags;
};

struct midlex_Tokenize midlex_tokenize(const char *src, const char *file);

void midlex_Tokenize_deinit(struct midlex_Tokenize *self);

#ifdef __cplusplus
}
#endif
