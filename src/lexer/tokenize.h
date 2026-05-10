#pragma once

#include "diag.h"
#include "literal.h"
#include "symbol.h"
#include "token.h"

struct Lexer_Tokenize {
    struct Lexer_TokenVec toks;
    struct SymbolTable symtbl;
    struct Literal_StringVec str_lits;
    struct DiagVec diags;
} Lexer_tokenize(const char *src, const char *file);

void Lexer_Tokenize_deinit(struct Lexer_Tokenize *self);
