#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "literal.h"
#include "parser/type.h"

enum Parser_ExprType {
    // num literals
    PARSER_EXPRTYPE_NUMLIT_START,
    PARSER_EXPRTYPE_CHAR_LIT,
    PARSER_EXPRTYPE_WCHAR_LIT,
    PARSER_EXPRTYPE_CHAR16_LIT,
    PARSER_EXPRTYPE_CHAR32_LIT,
    PARSER_EXPRTYPE_STRING_LIT,
    PARSER_EXPRTYPE_WSTRING_LIT,
    PARSER_EXPRTYPE_STRING16_LIT,
    PARSER_EXPRTYPE_STRING32_LIT,
    PARSER_EXPRTYPE_INT_LIT,
    PARSER_EXPRTYPE_UINT_LIT,
    PARSER_EXPRTYPE_LONG_LIT,
    PARSER_EXPRTYPE_ULONG_LIT,
    PARSER_EXPRTYPE_LONGLONG_LIT,
    PARSER_EXPRTYPE_ULONGLONG_LIT,
    PARSER_EXPRTYPE_FLOAT_LIT,
    PARSER_EXPRTYPE_DOUBLE_LIT,
    PARSER_EXPRTYPE_LONGDOUBLE_LIT,
    PARSER_EXPRTYPE_BOOL_LIT, // true, false
    PARSER_EXPRTYPE_PTR_LIT,  // nullptr
    PARSER_EXPRTYPE_NUMLIT_END,

    PARSER_EXPRTYPE_IDENTIFIER,

    // ternary ops
    PARSER_EXPRTYPE_TERNARYOP_START,
    PARSER_EXPRTYPE_CONDITIONAL,
    PARSER_EXPRTYPE_TERNARYOP_END,

    // binary ops
    PARSER_EXPRTYPE_BINOP_START,
    PARSER_EXPRTYPE_SCOPE_RES,
    PARSER_EXPRTYPE_MEMB_SEL,
    PARSER_EXPRTYPE_PTR_MEMB_SEL,
    PARSER_EXPRTYPE_ARRAY_SUBSCR,
    PARSER_EXPRTYPE_PTR_TO_MEMB_SEL,
    PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL,
    PARSER_EXPRTYPE_MUL,
    PARSER_EXPRTYPE_DIV,
    PARSER_EXPRTYPE_MOD,
    PARSER_EXPRTYPE_ADD,
    PARSER_EXPRTYPE_SUB,
    PARSER_EXPRTYPE_LEFT_SHIFT,
    PARSER_EXPRTYPE_RIGHT_SHIFT,
    PARSER_EXPRTYPE_LT,
    PARSER_EXPRTYPE_GT,
    PARSER_EXPRTYPE_LTEQ,
    PARSER_EXPRTYPE_GTEQ,
    PARSER_EXPRTYPE_EQ,
    PARSER_EXPRTYPE_NEQ,
    PARSER_EXPRTYPE_BITWISE_AND,
    PARSER_EXPRTYPE_BITWISE_XOR,
    PARSER_EXPRTYPE_BITWISE_OR,
    PARSER_EXPRTYPE_LOGICAL_AND,
    PARSER_EXPRTYPE_LOGICAL_OR,
    PARSER_EXPRTYPE_ASSIGN,
    PARSER_EXPRTYPE_MUL_ASSIGN,
    PARSER_EXPRTYPE_DIV_ASSIGN,
    PARSER_EXPRTYPE_MOD_ASSIGN,
    PARSER_EXPRTYPE_ADD_ASSIGN,
    PARSER_EXPRTYPE_SUB_ASSIGN,
    PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN,
    PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN,
    PARSER_EXPRTYPE_AND_ASSIGN,
    PARSER_EXPRTYPE_OR_ASSIGN,
    PARSER_EXPRTYPE_XOR_ASSIGN,
    PARSER_EXPRTYPE_COMMA,
    PARSER_EXPRTYPE_BINOP_END,

    // unary ops
    PARSER_EXPRTYPE_UNARYOP_START,
    PARSER_EXPRTYPE_FUNC_CALL, // not really a unary operator but close enough
    PARSER_EXPRTYPE_POSTFIX_INC,
    PARSER_EXPRTYPE_POSTFIX_DEC,
    PARSER_EXPRTYPE_TYPEID,
    PARSER_EXPRTYPE_CONSTCAST,
    PARSER_EXPRTYPE_DYNAMICCAST,
    PARSER_EXPRTYPE_REINTERPRETCAST,
    PARSER_EXPRTYPE_STATICCAST,
    PARSER_EXPRTYPE_SIZEOF,
    PARSER_EXPRTYPE_PREFIX_INC,
    PARSER_EXPRTYPE_PREFIX_DEC,
    PARSER_EXPRTYPE_BITWISE_NOT,
    PARSER_EXPRTYPE_LOGICAL_NOT,
    PARSER_EXPRTYPE_UNARY_PLUS,
    PARSER_EXPRTYPE_UNARY_MINUS,
    PARSER_EXPRTYPE_DEREF,
    PARSER_EXPRTYPE_REF,
    PARSER_EXPRTYPE_NEW,
    PARSER_EXPRTYPE_DELETE,
    PARSER_EXPRTYPE_CAST,
    PARSER_EXPRTYPE_THROW,
    PARSER_EXPRTYPE_UNARYOP_END,
};

bool Parser_is_numlit(enum Parser_ExprType type);
bool Parser_is_ternaryop(enum Parser_ExprType type);
bool Parser_is_binop(enum Parser_ExprType type);
bool Parser_is_unaryop(enum Parser_ExprType type);
bool Parser_is_op(enum Parser_ExprType type);

// goes from 0 to 15, where 15 is the highest precedence
i32 Parser_op_precedence(enum Parser_ExprType op);
bool Parser_op_ltr_assoc(enum Parser_ExprType op);

bool Parser_expr_uses_args(enum Parser_ExprType type);

enum Parser_ExprValueType {
    PARSER_EXPRVALUE_UNKNOWN,
    PARSER_EXPRVALUE_LVALUE,
    PARSER_EXPRVALUE_PRVALUE,
    PARSER_EXPRVALUE_XVALUE,
};

bool Parser_is_glvalue(enum Parser_ExprValueType type);
bool Parser_is_rvalue(enum Parser_ExprValueType type);

struct Parser_Expr;
gen_dynarray_struct_named(Parser_ExprVec, struct Parser_Expr);

struct Parser_Expr {
    union {
        struct Parser_ExprVec args;
        union Literal_Value val;
        const char *ident;
    } info;

    const struct Parser_ASTNode *node; // some expressions may have nodes
                                       // associated with them, like function
                                       // calls referencing the function being
                                       // called
    const struct Lexer_Token *tok;
    struct Parser_Type ret;
    enum Parser_ExprType type;
    enum Parser_ExprValueType valtype;
};

void Parser_Expr_deinit(struct Parser_Expr *expr);
// stops when reaching end_type
struct Parser_Expr Parser_parse_expr(const struct Lexer_Token *toks,
                                     isize_t start,
                                     const enum Lexer_TokenType *end_types,
                                     isize_t n_end_types, isize_t *out_end,
                                     struct DiagVec *diags);
// evaluate the result of a constant expression
union Literal_Value Parser_evaluate(const struct Parser_Expr *expr);
