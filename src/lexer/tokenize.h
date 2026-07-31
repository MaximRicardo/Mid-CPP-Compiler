#pragma once

#include "diag.h"
#include "literal.h"
#include "symbol.h"
#include "token.h"

struct MidLexer_Tokenize {
    struct MidLexer_TokenVec toks;
    struct MidSymbol_Table symtbl;
    struct MidLit_StringVec str_lits;
    struct MidDiag_DiagVec diags;
} MidLexer_tokenize(const char *src, const char *file);

void MidLexer_Tokenize_deinit(struct MidLexer_Tokenize *self);
