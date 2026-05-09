#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/scope.h"

enum Parser_ClassType {
    PARSER_CLASSTYPE_CLASS,
    PARSER_CLASSTYPE_STRUCT,
    PARSER_CLASSTYPE_UNION,
};

// classes, structs and unions
struct Parser_Class {
    struct Parser_ASTNodePVec pub_childs;  // public
    struct Parser_ASTNodePVec priv_childs; // private
    struct Parser_ASTNodePVec prot_childs; // protected
    const char *name;
    struct Parser_ASTNodePVec supers;    // classes this class inherits from
    const struct Lexer_Token *def_start; // the left curly '{'
    struct Sema_Scope *parent;
    i32 ident_idx; // index of the identifier holding the class in the parent
                   // scope. -1 if there is no identifier
    enum Parser_ClassType type;
    bool has_def;
};

void Parser_Class_deinit(struct Parser_Class *self);
struct Sema_Ident *Parser_class_ident(const struct Parser_Class *self);
// returns the end of the class
isize_t Parser_parse_class(struct Parser_Class *self,
                           struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           bool skip_def, struct Parser_Allocators *allocs,
                           struct DiagVec *diags);
// returns the end of the class body
isize_t Parser_parse_class_body(struct Parser_Class *self,
                                struct Parser_ASTNode *node,
                                const struct Lexer_Token *toks, isize_t l_curly,
                                struct Parser_Allocators *allocs,
                                struct DiagVec *diags);
