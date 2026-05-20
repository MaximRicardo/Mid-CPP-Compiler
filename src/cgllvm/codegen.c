#include "codegen.h"
#include "allocator.h"
#include "cgllvm/ident.h"
#include "cgllvm/name_mangle.h"
#include "cmd.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "literal.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/type.h"
#include "scope.h"
#include "type.h"
#include "types.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>
#include <stdio.h>
#include <string.h>

#if 0
void CGLLVM_Test(void)
{
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86AsmPrinter();

    LLVMModuleRef mod = LLVMModuleCreateWithName("test_module");

    LLVMTypeRef param_types[] = {LLVMInt32Type(), LLVMInt32Type()};
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types,
                                            ARRLEN(param_types), false);
    LLVMValueRef sum = LLVMAddFunction(mod, "sum", ret_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(sum, "entry");

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef tmp = LLVMBuildAdd(builder, LLVMGetParam(sum, 0),
                                    LLVMGetParam(sum, 1), "tmp");
    LLVMBuildRet(builder, tmp);

    LLVMDisposeBuilder(builder);

    char *error = NULL;
    if (LLVMVerifyModule(mod, LLVMAbortProcessAction, &error))
        printf("error: %s\n", error);
    LLVMDisposeMessage(error);

    {
        char *mod_str = LLVMPrintModuleToString(mod);
        printf("mod {\n%s\n}\n", mod_str);
        LLVMDisposeMessage(mod_str);
    }

    char *target_triple = LLVMGetDefaultTargetTriple();
    printf("target_triple = %s\n", target_triple);

    char *target_error = NULL;
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(target_triple, &target, &target_error))
        printf("target_error: %s\n", target_error);
    LLVMDisposeMessage(target_error);

    char cpu[] = "generic";
    char features[] = "";

    auto target_machine = LLVMCreateTargetMachine(
        target, target_triple, cpu, features, LLVMCodeGenLevelNone,
        LLVMRelocPIC, LLVMCodeModelDefault);

    auto target_data = LLVMCreateTargetDataLayout(target_machine);
    char *target_data_str = LLVMCopyStringRepOfTargetData(target_data);
    LLVMSetDataLayout(mod, target_data_str);
    LLVMSetTarget(mod, target_triple);

    char file_name[] = "../tests/output.s";

    char *emit_error = NULL;
    if (LLVMTargetMachineEmitToFile(target_machine, mod, file_name,
                                    LLVMAssemblyFile, &emit_error))
        printf("emit_error: %s\n", emit_error);
    LLVMDisposeMessage(emit_error);

    LLVMDisposeTargetData(target_data);
    LLVMDisposeMessage(target_data_str);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeMessage(target_triple);

    LLVMDisposeModule(mod);
}
#endif

void CGLLVM_init_codegen(void)
{
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86AsmPrinter();
}

static LLVMValueRef codegen_expr(const struct Parser_Expr *expr,
                                 struct CGLLVM_Scope *scope,
                                 LLVMContextRef context, LLVMModuleRef mod,
                                 LLVMBuilderRef builder);

static LLVMValueRef codegen_strlit(const struct Literal_String *lit,
                                   LLVMContextRef context, LLVMModuleRef mod)
{
    isize_t len = Literal_strlit_len(lit) + 1;
    int elem_size = lit->type == LITERAL_STRINGTYPE_CHAR ? Types_char_size * 8
                    : lit->type == LITERAL_STRINGTYPE_WCHAR
                        ? Types_wchar_size * 8
                    : lit->type == LITERAL_STRINGTYPE_CHAR16 ? 16
                                                             : 32;

    auto ret = LLVMAddGlobal(
        mod, LLVMArrayType2(LLVMIntTypeInContext(context, elem_size), len), "");

    LLVMSetLinkage(ret, LLVMInternalLinkage);
    LLVMSetGlobalConstant(ret, true);

    LLVMValueRef *elems = mid_malloc(len * sizeof(*elems));
    for (isize_t i = 0; i < len - 1; ++i) {
        switch (lit->type) {
        case LITERAL_STRINGTYPE_CHAR:
            elems[i] = LLVMConstInt(LLVMIntTypeInContext(context, elem_size),
                                    lit->c[i], Types_char_signed);
            break;

        case LITERAL_STRINGTYPE_WCHAR:
            elems[i] = LLVMConstInt(LLVMIntTypeInContext(context, elem_size),
                                    lit->wc[i], Types_wchar_signed);
            break;

        case LITERAL_STRINGTYPE_CHAR16:
            elems[i] = LLVMConstInt(LLVMIntTypeInContext(context, elem_size),
                                    lit->c16[i], false);
            break;

        case LITERAL_STRINGTYPE_CHAR32:
            elems[i] = LLVMConstInt(LLVMIntTypeInContext(context, elem_size),
                                    lit->c32[i], false);
            break;
        }
    }
    elems[len - 1] =
        LLVMConstInt(LLVMIntTypeInContext(context, elem_size), 0, false);

    LLVMSetInitializer(
        ret,
        LLVMConstArray2(LLVMIntTypeInContext(context, elem_size), elems, len));

    free(elems);
    return ret;
}

static LLVMValueRef codegen_lit_expr(const struct Parser_Expr *expr,
                                     LLVMContextRef context, LLVMModuleRef mod)
{
    if (Parser_is_signed_integral_typespec(expr->ret.spec))
        return LLVMConstInt(CGLLVM_convert_parser_type(&expr->ret, context),
                            expr->info.val.sint, true);
    else if (Parser_is_unsigned_integral_typespec(expr->ret.spec))
        return LLVMConstInt(CGLLVM_convert_parser_type(&expr->ret, context),
                            expr->info.val.uint, false);
    else if (Parser_is_floating_typespec(expr->ret.spec))
        return LLVMConstReal(CGLLVM_convert_parser_type(&expr->ret, context),
                             expr->info.val.flt);
    else if (Parser_is_strlit(expr->type))
        return codegen_strlit(&expr->info.val.str, context, mod);
    else if (expr->type == PARSER_EXPRTYPE_NULLPTR_LIT)
        return LLVMConstPointerNull(LLVMVoidType());
    else
        CRASH("expr isn't a literal");
}

static LLVMValueRef codegen_ident_expr(const struct Parser_Expr *expr,
                                       struct CGLLVM_Scope *scope,
                                       bool load_ref, LLVMBuilderRef builder)
{
    auto ident = CGLLVM_find_ident_const(scope, expr->info.ident, NULL);

    if (load_ref)
        return ident->val;
    else
        return LLVMBuildLoad2(builder, ident->type, ident->val, "");
}

static bool is_valid_array_subscr_ptr(const struct Parser_Type *type)
{
    return type->spec == PARSER_TYPESPEC_ARRAY || Parser_n_indir(type) > 0;
}

static LLVMValueRef codegen_subscr_expr(const struct Parser_Expr *expr,
                                        struct CGLLVM_Scope *scope,
                                        LLVMContextRef context,
                                        LLVMModuleRef mod,
                                        LLVMBuilderRef builder)
{
    auto lhs =
        codegen_expr(&expr->info.args.arr[0], scope, context, mod, builder);
    auto rhs =
        codegen_expr(&expr->info.args.arr[1], scope, context, mod, builder);

    auto lhs_t = &expr->info.args.arr[0].ret;
    auto rhs_t = &expr->info.args.arr[1].ret;
    bool lhs_is_array = is_valid_array_subscr_ptr(lhs_t);

    LLVMValueRef idxs[2] = {
        LLVMConstInt(LLVMInt32TypeInContext(context), 0, true),
        lhs_is_array ? rhs : lhs};

    return LLVMBuildGEP2(
        builder,
        CGLLVM_convert_parser_type(lhs_is_array ? lhs_t : rhs_t, context),
        lhs_is_array ? lhs : rhs, idxs, ARRLEN(idxs), "");
}

static LLVMValueRef cast_to_fp_expr_type(const struct Parser_Expr *expr,
                                         LLVMValueRef val, bool is_signed,
                                         LLVMContextRef context,
                                         LLVMBuilderRef builder)
{
    if (is_signed)
        return LLVMBuildSIToFP(
            builder, val, CGLLVM_convert_parser_type(&expr->ret, context), "");
    else
        return LLVMBuildUIToFP(
            builder, val, CGLLVM_convert_parser_type(&expr->ret, context), "");
}

static void cast_arith_expr_operands(const struct Parser_Expr *expr,
                                     LLVMValueRef *lhs, LLVMValueRef *rhs,
                                     LLVMContextRef context,
                                     LLVMBuilderRef builder)
{
    if (!Parser_is_binop(expr->type))
        return;
    if (LLVMGetTypeKind(LLVMTypeOf(*lhs)) == LLVMPointerTypeKind)
        return;
    if (LLVMGetTypeKind(LLVMTypeOf(*rhs)) == LLVMPointerTypeKind)
        return;

    auto lhs_expr = &expr->info.args.arr[0];
    auto rhs_expr = &expr->info.args.arr[1];

    if (Parser_is_integral_typespec(lhs_expr->ret.spec) &&
        Parser_is_floating_typespec(rhs_expr->ret.spec)) {
        *lhs = cast_to_fp_expr_type(
            expr, *lhs, Parser_is_signed_integral_typespec(lhs_expr->ret.spec),
            context, builder);
    } else if (Parser_is_integral_typespec(rhs_expr->ret.spec) &&
               Parser_is_floating_typespec(lhs_expr->ret.spec)) {
        *rhs = cast_to_fp_expr_type(
            expr, *rhs, Parser_is_signed_integral_typespec(rhs_expr->ret.spec),
            context, builder);
    }
}

static LLVMValueRef codegen_arith_expr(const struct Parser_Expr *expr,
                                       struct CGLLVM_Scope *scope,
                                       LLVMContextRef context,
                                       LLVMModuleRef mod,
                                       LLVMBuilderRef builder)
{
    auto lhs =
        codegen_expr(&expr->info.args.arr[0], scope, context, mod, builder);
    LLVMValueRef rhs;
    if (Parser_is_binop(expr->type))
        rhs =
            codegen_expr(&expr->info.args.arr[1], scope, context, mod, builder);

    cast_arith_expr_operands(expr, &lhs, &rhs, context, builder);

    switch (expr->type) {
    case PARSER_EXPRTYPE_MUL:
        return LLVMBuildMul(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_DIV:
        // printf("lhs is float = %d\n", lhs);
        if (Parser_is_floating_typespec(expr->ret.spec))
            return LLVMBuildFDiv(builder, lhs, rhs, "");
        else if (Parser_is_signed_integral_typespec(expr->ret.spec))
            return LLVMBuildSDiv(builder, lhs, rhs, "");
        else
            return LLVMBuildUDiv(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_MOD:
        if (Parser_is_floating_typespec(expr->ret.spec))
            return LLVMBuildFRem(builder, lhs, rhs, "");
        else if (Parser_is_signed_integral_typespec(expr->ret.spec))
            return LLVMBuildSRem(builder, lhs, rhs, "");
        else
            return LLVMBuildURem(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_ADD:
        return LLVMBuildAdd(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_SUB:
        return LLVMBuildSub(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_LEFT_SHIFT:
        return LLVMBuildShl(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_RIGHT_SHIFT:
        if (Parser_is_signed_integral_typespec(expr->ret.spec))
            return LLVMBuildAShr(builder, lhs, rhs, "");
        else
            return LLVMBuildLShr(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_BITWISE_AND:
        return LLVMBuildAnd(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_BITWISE_XOR:
        return LLVMBuildXor(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_BITWISE_OR:
        return LLVMBuildOr(builder, lhs, rhs, "");

    case PARSER_EXPRTYPE_BITWISE_NOT:
        return LLVMBuildNot(builder, lhs, "");

    case PARSER_EXPRTYPE_UNARY_PLUS:
        return lhs;

    case PARSER_EXPRTYPE_UNARY_MINUS:
        return LLVMBuildNeg(builder, lhs, "");

    default:
        CRASH("expr isn't an arithmetic operator");
    }
}

static LLVMValueRef codegen_expr(const struct Parser_Expr *expr,
                                 struct CGLLVM_Scope *scope,
                                 LLVMContextRef context, LLVMModuleRef mod,
                                 LLVMBuilderRef builder)
{
    if (Parser_is_numlit(expr->type))
        return codegen_lit_expr(expr, context, mod);
    if (expr->type == PARSER_EXPRTYPE_IDENTIFIER)
        return codegen_ident_expr(expr, scope, false, builder);
    if (expr->type == PARSER_EXPRTYPE_ARRAY_SUBSCR)
        return codegen_subscr_expr(expr, scope, context, mod, builder);
    else if (Parser_is_arith_op(expr->type))
        return codegen_arith_expr(expr, scope, context, mod, builder);
    else {
        printf("expr at %" PRIi32 ":%" PRIi32 "\n", expr->tok->pos.line,
               expr->tok->pos.column);
        CRASH("bad expr type\n");
    }
}

static void codegen_node(const struct Parser_ASTNode *node,
                         struct CGLLVM_Scope *scope,
                         struct CGLLVM_Allocators *allocs,
                         LLVMContextRef context, LLVMModuleRef mod,
                         LLVMBuilderRef builder);

static struct CGLLVM_Scope *create_func_scope(struct CGLLVM_Scope *scope,
                                              struct CGLLVM_Allocators *allocs,
                                              const struct Parser_ASTNode *node)
{
    struct CGLLVM_Scope *ret;
    gen_bumpmalloc(&allocs->scope, &ret);
    *ret = (struct CGLLVM_Scope){.parent = scope, .node = node};
    return ret;
}

static void codegen_func_body(const struct Parser_ASTNode *node,
                              struct CGLLVM_Scope *parent_scope,
                              struct CGLLVM_Allocators *allocs,
                              LLVMValueRef func, LLVMContextRef context,
                              LLVMModuleRef mod)
{
    auto scope = create_func_scope(parent_scope, allocs, node);

    LLVMBasicBlockRef entry =
        LLVMAppendBasicBlockInContext(context, func, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    for (isize_t i = 0; i < node->func_decl.nodes.len; ++i) {
        auto child = node->func_decl.nodes.arr[i];

        codegen_node(child, scope, allocs, context, mod, builder);
    }

    LLVMBuildRet(builder,
                 LLVMConstInt(LLVMInt32TypeInContext(context), 0, true));
    LLVMDisposeBuilder(builder);
}

static void codegen_func_node(const struct Parser_ASTNode *node,
                              struct CGLLVM_Scope *scope,
                              struct CGLLVM_Allocators *allocs,
                              LLVMContextRef context, LLVMModuleRef mod)
{
    LLVMTypeRef *params =
        mid_malloc(node->func_decl.params.len * sizeof(*params));
    for (isize_t i = 0; i < node->func_decl.params.len; ++i) {
        params[i] = CGLLVM_convert_parser_type(
            &node->func_decl.params.arr[i]->var_decl.insts.arr[0].type,
            context);
    }

    struct CGLLVM_Ident ident = {};

    ident.type = LLVMFunctionType(
        CGLLVM_convert_parser_type(&node->func_decl.type, context), params,
        node->func_decl.params.len, node->func_decl.variadic);
    free(params);

    ident.name = CGLLVM_mangle_func(&node->func_decl);
    ident.val = LLVMAddFunction(mod, ident.name, ident.type);

    gen_dynpush(&scope->idents, ident);

    if (node->func_decl.nodes.len > 0)
        codegen_func_body(node, scope, allocs, ident.val, context, mod);
}

static void codegen_var_inst_node(const struct Parser_VarDeclInst *inst,
                                  struct CGLLVM_Scope *scope,
                                  LLVMContextRef context, LLVMModuleRef mod,
                                  LLVMBuilderRef builder)
{
    if (!inst->name)
        return;

    assert(!inst->has_ctor);

    struct CGLLVM_Ident ident = {.name = strdup(inst->name)};

    ident.type = CGLLVM_convert_parser_type(&inst->type, context);
    ident.val = LLVMBuildAlloca(builder, ident.type, inst->name);

    if (inst->init.expr) {
        auto init = codegen_expr(inst->init.expr, scope, context, mod, builder);
        LLVMBuildStore(builder, init, ident.val);
    }

    gen_dynpush(&scope->idents, ident);
}

static void codegen_var_node(const struct Parser_ASTNode *node,
                             struct CGLLVM_Scope *scope, LLVMContextRef context,
                             LLVMModuleRef mod, LLVMBuilderRef builder)
{
    for (isize_t i = 0; i < node->var_decl.insts.len; ++i)
        codegen_var_inst_node(&node->var_decl.insts.arr[i], scope, context, mod,
                              builder);
}

static void codegen_node(const struct Parser_ASTNode *node,
                         struct CGLLVM_Scope *scope,
                         struct CGLLVM_Allocators *allocs,
                         LLVMContextRef context, LLVMModuleRef mod,
                         LLVMBuilderRef builder)
{
    (void)builder;

    switch (node->type) {
    case PARSER_ASTNODETYPE_ROOT:
        CRASH("root node encountered within AST");

    case PARSER_ASTNODETYPE_FUNC_DECL:
        codegen_func_node(node, scope, allocs, context, mod);
        break;

    case PARSER_ASTNODETYPE_VAR_DECL:
        codegen_var_node(node, scope, context, mod, builder);
        break;

    case PARSER_ASTNODETYPE_EXPR:
        codegen_expr(&node->expr, scope, context, mod, builder);

    default:
        break;
    }
}

static void verify_module(LLVMModuleRef mod)
{
    char *error = NULL;
    if (LLVMVerifyModule(mod, LLVMAbortProcessAction, &error))
        printf("error: %s\n", error);
    LLVMDisposeMessage(error);
}

static void print_module(LLVMModuleRef mod)
{
    char *mod_str = LLVMPrintModuleToString(mod);
    printf("mod {\n%s\n}\n", mod_str);
    LLVMDisposeMessage(mod_str);
}

void CGLLVM_codegen(const struct Parser_ASTNode *root)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef mod =
        LLVMModuleCreateWithNameInContext(CMD_get_args()->src, context);

    struct CGLLVM_Allocators allocs = {};
    struct CGLLVM_Scope root_scope = {};

    for (isize_t i = 0; i < root->root.len; ++i)
        codegen_node(root->root.arr[i], &root_scope, &allocs, context, mod,
                     NULL);

    CGLLVM_Scope_deinit(&root_scope);
    CGLLVM_Allocators_deinit(&allocs);

    verify_module(mod);
    print_module(mod);

    LLVMDisposeModule(mod);
    LLVMContextDispose(context);
}
