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
#include "parser/class.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "scope.h"
#include "sema/scope.h"
#include "type.h"
#include "types.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void print_module(LLVMModuleRef mod)
{
    char *mod_str = LLVMPrintModuleToString(mod);
    printf("mod {\n%s\n}\n", mod_str);
    LLVMDisposeMessage(mod_str);
}

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
                                 const struct CGLLVM_Scope *scope,
                                 LLVMContextRef context, LLVMModuleRef mod,
                                 LLVMBuilderRef builder);
static LLVMValueRef codegen_expr_ref(const struct Parser_Expr *expr,
                                     const struct CGLLVM_Scope *scope,
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
                                       const struct CGLLVM_Scope *scope,
                                       bool load_ref, LLVMBuilderRef builder)
{
    auto ident = CGLLVM_find_ident_const(scope, expr->info.ident, NULL);

    if (load_ref)
        return ident->val;
    else
        return LLVMBuildLoad2(builder, ident->type, ident->val, "");
}

static LLVMValueRef get_implicit_this(const struct CGLLVM_Scope *scope)
{
    auto func = CGLLVM_find_func_scope_const(scope);
    auto f_ident = &func->parent->idents.arr[func->ident_idx];

    return LLVMGetParam(f_ident->val, 0);
}

static bool is_valid_array_subscr_ptr(const struct Parser_Type *type)
{
    return type->spec == PARSER_TYPESPEC_ARRAY || Parser_n_indir(type) > 0;
}

static LLVMValueRef codegen_subscr_expr(const struct Parser_Expr *expr,
                                        const struct CGLLVM_Scope *scope,
                                        bool load_ref, LLVMContextRef context,
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

    auto type =
        CGLLVM_convert_parser_type(lhs_is_array ? lhs_t : rhs_t, context);

    auto ptr = LLVMBuildGEP2(builder, type, lhs_is_array ? lhs : rhs,
                             lhs_is_array ? &rhs : &lhs, 1, "");

    if (load_ref)
        return ptr;

    return LLVMBuildLoad2(builder, type, ptr, "");
}

static LLVMValueRef cast_value(LLVMValueRef val, const struct Parser_Type *src,
                               const struct Parser_Type *dest,
                               LLVMContextRef context, LLVMBuilderRef builder)
{
    if (Parser_are_types_same(src, dest))
        return val;

    isize_t src_indir = Parser_n_indir(src);
    isize_t dest_indir = Parser_n_indir(dest);

    // opaque ptrs are used so ptrs of different types shouldn't be an issue
    if (src_indir > 0 && dest_indir > 0)
        return val;

    bool src_signed = Parser_is_signed_integral_typespec(src->spec);
    bool src_int = Parser_is_integral_typespec(src->spec);
    bool src_fp = Parser_is_floating_typespec(src->spec);

    bool dest_int = Parser_is_integral_typespec(dest->spec);
    bool dest_fp = Parser_is_floating_typespec(dest->spec);

    LLVMTypeRef dest_type = CGLLVM_convert_parser_type(dest, context);

    if (src_indir == 0 && src_int && dest_indir > 0) {
        return LLVMBuildIntToPtr(builder, val, dest_type, "");
    } else if (src_indir > 0 && dest_indir == 0 && dest_int) {
        return LLVMBuildPtrToInt(builder, val, dest_type, "");
    } else if (src_indir > 0 || dest_indir > 0) {
        CRASH("invalid ptr conversion");
    }

    if (src_int && dest_fp) {
        if (src_signed)
            return LLVMBuildSIToFP(builder, val, dest_type, "");
        else
            return LLVMBuildUIToFP(builder, val, dest_type, "");
    } else if (src_int && dest_int) {
        if (src_signed)
            return LLVMBuildSExt(builder, val, dest_type, "");
        else
            return LLVMBuildZExt(builder, val, dest_type, "");
    } else if (src_fp && dest_fp) {
        return LLVMBuildFPExt(builder, val, dest_type, "");
    }

    CRASH("couldn't cast assignment operand");
}

/*
static LLVMValueRef cast_arith_expr_operand(LLVMValueRef val,
                                            const struct Parser_Type *src,
                                            const struct Parser_Type *dest,
                                            LLVMContextRef context,
                                            LLVMBuilderRef builder)
{
    bool src_signed = Parser_is_signed_integral_typespec(src->spec);
    bool src_int = Parser_is_integral_typespec(src->spec);
    bool src_fp = Parser_is_floating_typespec(src->spec);
    i32 src_rank = Parser_typespec_conv_rank(src->spec);

    bool dest_int = Parser_is_integral_typespec(dest->spec);
    bool dest_fp = Parser_is_floating_typespec(dest->spec);
    i32 dest_rank = Parser_typespec_conv_rank(dest->spec);

    LLVMTypeRef dest_type = CGLLVM_convert_parser_type(dest, context);

    if (src_int && dest_fp) {
        if (src_signed)
            return LLVMBuildSIToFP(builder, val, dest_type, "");
        else
            return LLVMBuildUIToFP(builder, val, dest_type, "");
    } else if (src_int && dest_int && src_rank < dest_rank) {
        if (src_signed)
            return LLVMBuildSExt(builder, val, dest_type, "");
        else
            return LLVMBuildZExt(builder, val, dest_type, "");
    } else if (src_fp && dest_fp && src_rank < dest_rank) {
        return LLVMBuildFPExt(builder, val, dest_type, "");
    }

    return val;
}
*/

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

    *lhs = cast_value(*lhs, &lhs_expr->ret, &expr->ret, context, builder);
    *rhs = cast_value(*rhs, &rhs_expr->ret, &expr->ret, context, builder);
}

// add    - if true, lhs + rhs is generated, else, lhs - rhs is generated.
static LLVMValueRef codegen_ptr_arith_expr(const struct Parser_Expr *expr,
                                           LLVMValueRef lhs, LLVMValueRef rhs,
                                           bool add, LLVMContextRef context,
                                           LLVMBuilderRef builder)
{
    assert(Parser_n_indir(&expr->ret) > 0);

    bool lhs_ptr = Parser_n_indir(&expr->info.args.arr[0].ret) > 0;
    LLVMValueRef ptr = lhs_ptr ? lhs : rhs;
    LLVMValueRef off = lhs_ptr ? rhs : lhs;

    if (!add)
        off = LLVMBuildNeg(builder, off, "");

    auto deref = Parser_deref_type(&expr->ret, NULL);
    auto ret = LLVMBuildGEP2(
        builder, CGLLVM_convert_parser_type(&deref, context), ptr, &off, 1, "");

    Parser_Type_deinit(&deref);
    return ret;
}

static LLVMValueRef codegen_arith_expr(const struct Parser_Expr *expr,
                                       const struct CGLLVM_Scope *scope,
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
        if (Parser_n_indir(&expr->ret) == 0)
            return LLVMBuildAdd(builder, lhs, rhs, "");
        else
            return codegen_ptr_arith_expr(expr, lhs, rhs, true, context,
                                          builder);

    case PARSER_EXPRTYPE_SUB:
        if (Parser_n_indir(&expr->ret) == 0)
            return LLVMBuildSub(builder, lhs, rhs, "");
        else
            return codegen_ptr_arith_expr(expr, lhs, rhs, false, context,
                                          builder);

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

static LLVMValueRef *get_call_args(const struct Parser_Expr *expr,
                                   const struct CGLLVM_Scope *scope,
                                   LLVMContextRef context, LLVMModuleRef mod,
                                   LLVMBuilderRef builder, isize_t *out_n_args)
{
    bool implicit_this =
        Parser_func_takes_implicit_this(&expr->node->func_decl, true);

    isize_t n_args = expr->info.args.len - 1 + implicit_this;
    if (out_n_args)
        *out_n_args = n_args;

    LLVMValueRef *ret = mid_malloc(n_args * sizeof(*ret));

    for (isize_t i = 0; i < expr->info.args.len - 1; ++i) {
        auto arg = &expr->info.args.arr[i + 1];
        auto param =
            &expr->node->func_decl.params.arr[i]->var_decl.insts.arr[0];

        auto val = codegen_expr(arg, scope, context, mod, builder);
        val = cast_value(val, &arg->ret, &param->type, context, builder);

        ret[i + implicit_this] = val;
    }

    if (Parser_func_is_ctor(&expr->node->func_decl)) {
        auto type = Parser_implicit_this_type(&expr->node->func_decl);
        ret[0] = LLVMBuildAlloca(
            builder, CGLLVM_convert_parser_type(&type, context), "");
        Parser_Type_deinit(&type);
    } else if (implicit_this) {
        // jank
        auto lhs = &expr->info.args.arr[0];
        assert(Parser_is_memb_sel(lhs->type));
        ret[0] = codegen_expr_ref(&lhs->info.args.arr[0], scope, context, mod,
                                  builder);
    }

    return ret;
}

static LLVMValueRef codegen_call_expr(const struct Parser_Expr *expr,
                                      const struct CGLLVM_Scope *scope,
                                      LLVMContextRef context, LLVMModuleRef mod,
                                      LLVMBuilderRef builder)
{
    char *name = CGLLVM_mangle_func(&expr->node->func_decl);

    auto root = CGLLVM_find_root_scope_const(scope);
    auto func = CGLLVM_find_ident_const(root, name, NULL);

    isize_t n_args;
    LLVMValueRef *args =
        get_call_args(expr, scope, context, mod, builder, &n_args);

    auto ret = LLVMBuildCall2(builder, func->type, func->val, args, n_args, "");
    // ctors return the resulting value
    if (Parser_func_is_ctor(&expr->node->func_decl)) {
        auto type = Parser_implicit_this_type(&expr->node->func_decl);
        ret = LLVMBuildLoad2(
            builder, CGLLVM_convert_parser_type(&type, context), args[0], "");
        Parser_Type_deinit(&type);
    }

    free(args);
    free(name);
    return ret;
}

static LLVMValueRef codegen_memb_sel(const struct Parser_Expr *expr,
                                     const struct CGLLVM_Scope *scope,
                                     bool load_ref, LLVMContextRef context,
                                     LLVMModuleRef mod, LLVMBuilderRef builder)
{
    const struct Parser_Expr *lhs_expr = &expr->info.args.arr[0];
    const struct Parser_Expr *rhs_expr = &expr->info.args.arr[1];

    LLVMValueRef lhs;
    if (expr->type == PARSER_EXPRTYPE_PTR_MEMB_SEL)
        lhs = codegen_expr(lhs_expr, scope, context, mod, builder);
    else
        lhs = codegen_expr_ref(lhs_expr, scope, context, mod, builder);

    assert(rhs_expr->type == PARSER_EXPRTYPE_IDENTIFIER);

    auto class_ = &Parser_named_type_ident(&lhs_expr->ret.named)->decl->class_;
    isize_t idx =
        CGLLVM_class_field_to_struct_field_idx(class_, rhs_expr->info.ident);
    if (idx == -1)
        return NULL;

    auto struct_type = CGLLVM_create_struct(
        &Parser_named_type_ident(&lhs_expr->ret.named)->decl->class_, context);
    LLVMValueRef ptr_idxs[2] = {
        LLVMConstNull(LLVMInt32TypeInContext(context)),
        LLVMConstInt(LLVMInt32TypeInContext(context), idx, true)};
    auto ptr = LLVMBuildGEP2(builder, struct_type, lhs, ptr_idxs,
                             ARRLEN(ptr_idxs), "");
    if (load_ref)
        return ptr;

    auto res_t = CGLLVM_convert_parser_type(&expr->ret, context);
    return LLVMBuildLoad2(builder, res_t, ptr, "");
}

static LLVMValueRef codegen_assign_expr(const struct Parser_Expr *expr,
                                        const struct CGLLVM_Scope *scope,
                                        bool load_ref, LLVMContextRef context,
                                        LLVMModuleRef mod,
                                        LLVMBuilderRef builder)
{
    const struct Parser_Expr *lhs_expr = &expr->info.args.arr[0];
    const struct Parser_Expr *rhs_expr = &expr->info.args.arr[1];

    auto lhs = codegen_expr_ref(lhs_expr, scope, context, mod, builder);
    auto rhs = codegen_expr(rhs_expr, scope, context, mod, builder);

    LLVMValueRef deref_lhs;
    if (load_ref || expr->type != PARSER_EXPRTYPE_ASSIGN) {
        deref_lhs = LLVMBuildLoad2(
            builder, CGLLVM_convert_parser_type(&lhs_expr->ret, context), lhs,
            "");
    }

    LLVMValueRef res;
    switch (expr->type) {
    case PARSER_EXPRTYPE_ASSIGN:
        res = rhs;
        break;

    case PARSER_EXPRTYPE_MUL_ASSIGN:
        res = LLVMBuildMul(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_DIV_ASSIGN:
        if (Parser_is_floating_typespec(expr->ret.spec))
            res = LLVMBuildFDiv(builder, deref_lhs, rhs, "");
        else if (Parser_is_signed_integral_typespec(expr->ret.spec))
            res = LLVMBuildSDiv(builder, deref_lhs, rhs, "");
        else
            res = LLVMBuildUDiv(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_MOD_ASSIGN:
        if (Parser_is_floating_typespec(expr->ret.spec))
            res = LLVMBuildFRem(builder, deref_lhs, rhs, "");
        else if (Parser_is_signed_integral_typespec(expr->ret.spec))
            res = LLVMBuildSRem(builder, deref_lhs, rhs, "");
        else
            res = LLVMBuildURem(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_ADD_ASSIGN:
        if (Parser_n_indir(&expr->ret) == 0)
            res = LLVMBuildAdd(builder, deref_lhs, rhs, "");
        else
            res = codegen_ptr_arith_expr(expr, deref_lhs, rhs, true, context,
                                         builder);
        break;

    case PARSER_EXPRTYPE_SUB_ASSIGN:
        if (Parser_n_indir(&expr->ret) == 0)
            res = LLVMBuildSub(builder, deref_lhs, rhs, "");
        else
            res = codegen_ptr_arith_expr(expr, deref_lhs, rhs, false, context,
                                         builder);
        break;

    case PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        res = LLVMBuildShl(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        if (Parser_is_signed_integral_typespec(expr->ret.spec))
            res = LLVMBuildAShr(builder, deref_lhs, rhs, "");
        else
            res = LLVMBuildLShr(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_AND_ASSIGN:
        res = LLVMBuildAnd(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_XOR_ASSIGN:
        res = LLVMBuildXor(builder, deref_lhs, rhs, "");
        break;

    case PARSER_EXPRTYPE_OR_ASSIGN:
        res = LLVMBuildOr(builder, deref_lhs, rhs, "");
        break;

    default:
        CRASH("not an assignment expr");
    }

    LLVMBuildStore(builder, res, lhs);
    return load_ref ? lhs : res;
}

// loads in a reference to the result, like in the expression &p[10], the value
// p + 10 is returned
static LLVMValueRef codegen_expr_ref(const struct Parser_Expr *expr,
                                     const struct CGLLVM_Scope *scope,
                                     LLVMContextRef context, LLVMModuleRef mod,
                                     LLVMBuilderRef builder)
{
    assert(!expr->overloaded);

    if (expr->type == PARSER_EXPRTYPE_IDENTIFIER)
        return codegen_ident_expr(expr, scope, true, builder);
    else if (expr->type == PARSER_EXPRTYPE_THIS)
        return get_implicit_this(scope);
    else if (expr->type == PARSER_EXPRTYPE_ARRAY_SUBSCR)
        return codegen_subscr_expr(expr, scope, true, context, mod, builder);
    else if (Parser_is_memb_sel(expr->type))
        return codegen_memb_sel(expr, scope, true, context, mod, builder);
    else if (Parser_is_assignment(expr->type))
        return codegen_assign_expr(expr, scope, false, context, mod, builder);
    else
        CRASH("can't get ref of expr type");
}

static LLVMValueRef codegen_expr(const struct Parser_Expr *expr,
                                 const struct CGLLVM_Scope *scope,
                                 LLVMContextRef context, LLVMModuleRef mod,
                                 LLVMBuilderRef builder)
{
    assert(!expr->overloaded &&
           "codegen of overloaded exprs not implemented yet");

    if (Parser_is_numlit(expr->type))
        return codegen_lit_expr(expr, context, mod);
    else if (expr->type == PARSER_EXPRTYPE_IDENTIFIER)
        return codegen_ident_expr(expr, scope, false, builder);
    else if (expr->type == PARSER_EXPRTYPE_THIS)
        return get_implicit_this(scope);
    else if (expr->type == PARSER_EXPRTYPE_ARRAY_SUBSCR)
        return codegen_subscr_expr(expr, scope, false, context, mod, builder);
    else if (Parser_is_arith_op(expr->type))
        return codegen_arith_expr(expr, scope, context, mod, builder);
    else if (expr->type == PARSER_EXPRTYPE_FUNC_CALL)
        return codegen_call_expr(expr, scope, context, mod, builder);
    else if (Parser_is_memb_sel(expr->type))
        return codegen_memb_sel(expr, scope, false, context, mod, builder);
    else if (Parser_is_assignment(expr->type))
        return codegen_assign_expr(expr, scope, false, context, mod, builder);
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

static struct CGLLVM_Scope *create_scope(struct CGLLVM_Scope *scope,
                                         struct CGLLVM_Allocators *allocs,
                                         const struct Parser_ASTNode *node)
{
    struct CGLLVM_Scope *ret;
    gen_bumpmalloc(&allocs->scope, &ret);
    *ret =
        (struct CGLLVM_Scope){.parent = scope, .node = node, .ident_idx = -1};

    return ret;
}

static void add_params_to_func(const struct Parser_FuncDecl *func,
                               struct CGLLVM_Scope *scope,
                               LLVMContextRef context, LLVMValueRef func_val,
                               LLVMBuilderRef builder)
{
    for (isize_t i = 0; i < func->params.len; ++i) {
        auto param = &func->params.arr[i]->var_decl.insts.arr[0];

        struct CGLLVM_Ident ident = {.name = strdup(param->name)};

        ident.type = CGLLVM_convert_parser_type(&param->type, context);
        ident.val = LLVMBuildAlloca(builder, ident.type, "");
        LLVMBuildStore(builder, LLVMGetParam(func_val, i), ident.val);

        gen_dynpush(&scope->idents, ident);
    }
}

static struct CGLLVM_Scope *
create_func_scope(struct CGLLVM_Scope *scope, struct CGLLVM_Allocators *allocs,
                  const struct Parser_ASTNode *node, isize_t func_ident,
                  LLVMContextRef context, LLVMValueRef func_val,
                  LLVMBuilderRef builder)
{
    struct CGLLVM_Scope *ret = create_scope(scope, allocs, node);
    ret->ident_idx = func_ident;

    add_params_to_func(&node->func_decl, ret, context, func_val, builder);

    return ret;
}

static void codegen_func_body(const struct Parser_ASTNode *node,
                              isize_t func_ident,
                              struct CGLLVM_Scope *parent_scope,
                              struct CGLLVM_Allocators *allocs,
                              LLVMValueRef func, LLVMContextRef context,
                              LLVMModuleRef mod)
{
    LLVMBasicBlockRef entry =
        LLVMAppendBasicBlockInContext(context, func, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    auto scope = create_func_scope(parent_scope, allocs, node, func_ident,
                                   context, func, builder);

    bool ret_fnd = false;

    for (isize_t i = 0; i < node->func_decl.nodes.len; ++i) {
        auto child = node->func_decl.nodes.arr[i];

        codegen_node(child, scope, allocs, context, mod, builder);

        if (child->type == PARSER_ASTNODETYPE_RETURN) {
            ret_fnd = true;
            break;
        }
    }

    if (!ret_fnd)
        LLVMBuildRet(builder,
                     LLVMConstInt(LLVMInt32TypeInContext(context), 0, true));

    LLVMDisposeBuilder(builder);
}

static LLVMTypeRef *get_func_params(const struct Parser_ASTNode *node,
                                    LLVMContextRef context,
                                    isize_t *out_n_params)
{
    bool implicit_this =
        Parser_func_takes_implicit_this(&node->func_decl, true);
    isize_t n_params = node->func_decl.params.len + implicit_this;
    if (out_n_params)
        *out_n_params = n_params;

    LLVMTypeRef *params = mid_malloc(n_params * sizeof(*params));
    for (isize_t i = 0; i < node->func_decl.params.len; ++i) {
        params[i + implicit_this] = CGLLVM_convert_parser_type(
            &node->func_decl.params.arr[i]->var_decl.insts.arr[0].type,
            context);
    }

    if (implicit_this)
        params[0] = LLVMPointerTypeInContext(context, 0);

    return params;
}

static void codegen_func_node(const struct Parser_ASTNode *node,
                              struct CGLLVM_Scope *scope,
                              struct CGLLVM_Allocators *allocs,
                              LLVMContextRef context, LLVMModuleRef mod)
{
    isize_t n_params;
    LLVMTypeRef *params = get_func_params(node, context, &n_params);

    struct CGLLVM_Ident ident = {};

    auto ret_type =
        node->func_decl.is_tor
            ? LLVMVoidTypeInContext(context)
            : CGLLVM_convert_parser_type(&node->func_decl.type, context);
    ident.type =
        LLVMFunctionType(ret_type, params, n_params, node->func_decl.variadic);
    free(params);

    ident.name = CGLLVM_mangle_func(&node->func_decl);
    ident.val = LLVMAddFunction(mod, ident.name, ident.type);

    gen_dynpush(&scope->idents, ident);

    if (node->func_decl.nodes.len > 0)
        codegen_func_body(node, scope->idents.len - 1, scope, allocs, ident.val,
                          context, mod);
}

static void call_ctor(const struct Parser_VarDeclInst *inst,
                      const struct CGLLVM_Ident *ident,
                      struct CGLLVM_Scope *scope, LLVMContextRef context,
                      LLVMModuleRef mod, LLVMBuilderRef builder)
{
    isize_t n_args = inst->ctor.args.len + 1;
    LLVMValueRef *args = mid_malloc(n_args * sizeof(*args));

    args[0] = ident->val;

    for (isize_t i = 0; i < inst->ctor.args.len; ++i) {
        auto arg = &args[i + 1];
        auto arg_expr = &inst->ctor.args.arr[i];
        auto param =
            &inst->ctor.node->func_decl.params.arr[i]->var_decl.insts.arr[0];

        *arg = codegen_expr(arg_expr, scope, context, mod, builder);
        *arg = cast_value(*arg, &arg_expr->ret, &param->type, context, builder);
    }

    char *name = CGLLVM_mangle_func(&inst->ctor.node->func_decl);
    auto root = CGLLVM_find_root_scope_const(scope);
    auto func = CGLLVM_find_ident_const(root, name, NULL);

    LLVMBuildCall2(builder, func->type, func->val, args, n_args, "");

    free(name);
    free(args);
}

static void codegen_var_inst_node(const struct Parser_VarDeclInst *inst,
                                  struct CGLLVM_Scope *scope,
                                  LLVMContextRef context, LLVMModuleRef mod,
                                  LLVMBuilderRef builder)
{
    if (!inst->name)
        return;

    struct CGLLVM_Ident ident = {.name = strdup(inst->name)};

    ident.type = CGLLVM_convert_parser_type(&inst->type, context);
    ident.val = LLVMBuildAlloca(builder, ident.type, inst->name);
    gen_dynpush(&scope->idents, ident);

    if (inst->has_ctor) {
        call_ctor(inst, &ident, scope, context, mod, builder);
    } else if (inst->init.expr) {
        auto init = codegen_expr(inst->init.expr, scope, context, mod, builder);
        init = cast_value(init, &inst->init.expr->ret, &inst->type, context,
                          builder);
        LLVMBuildStore(builder, init, ident.val);
    }
}

static void codegen_var_node(const struct Parser_ASTNode *node,
                             struct CGLLVM_Scope *scope, LLVMContextRef context,
                             LLVMModuleRef mod, LLVMBuilderRef builder)
{
    for (isize_t i = 0; i < node->var_decl.insts.len; ++i)
        codegen_var_inst_node(&node->var_decl.insts.arr[i], scope, context, mod,
                              builder);
}

static void codegen_ret_node(const struct Parser_ASTNode *node,
                             struct CGLLVM_Scope *scope, LLVMContextRef context,
                             LLVMModuleRef mod, LLVMBuilderRef builder)
{
    if (node->ret.expr) {
        auto val = codegen_expr(node->ret.expr, scope, context, mod, builder);
        LLVMBuildRet(builder, val);
    } else {
        LLVMBuildRetVoid(builder);
    }
}

static void codegen_class_methods(const struct Parser_ASTNode *node,
                                  struct CGLLVM_Scope *scope,
                                  struct CGLLVM_Allocators *allocs,
                                  LLVMContextRef context, LLVMModuleRef mod)
{
    for (isize_t i = 0; i < node->class_.childs.len; ++i) {
        auto child = node->class_.childs.arr[i];

        if (child->type != PARSER_ASTNODETYPE_FUNC_DECL)
            continue;

        codegen_func_node(child, scope, allocs, context, mod);
    }
}

static void codegen_class_node(const struct Parser_ASTNode *node,
                               struct CGLLVM_Scope *scope,
                               struct CGLLVM_Allocators *allocs,
                               LLVMContextRef context, LLVMModuleRef mod)
{
    /*
    char *name = CGLLVM_named_type_full_name(&(struct Parser_TypeNamed){
        .parent = node->class_.parent, .ident = node->class_.ident_idx});

    auto type = LLVMStructCreateNamed(context, name);
    auto fields = CGLLVM_class_to_struct_fields(&node->class_, context);
    LLVMStructSetBody(type, fields.arr, fields.len, false);

    gen_dyndeinit(&fields);
    free(name);
    */

    codegen_class_methods(node, scope, allocs, context, mod);
}

static void codegen_nmspace_node(const struct Parser_ASTNode *node,
                                 struct CGLLVM_Scope *scope,
                                 struct CGLLVM_Allocators *allocs,
                                 LLVMContextRef context, LLVMModuleRef mod)
{
    for (isize_t i = 0; i < node->nmspace.childs.len; ++i) {
        auto child = node->nmspace.childs.arr[i];

        codegen_node(child, scope, allocs, context, mod, NULL);
    }
}

static void codegen_node(const struct Parser_ASTNode *node,
                         struct CGLLVM_Scope *scope,
                         struct CGLLVM_Allocators *allocs,
                         LLVMContextRef context, LLVMModuleRef mod,
                         LLVMBuilderRef builder)
{
    switch (node->type) {
    case PARSER_ASTNODETYPE_ROOT:
        CRASH("root node encountered within AST");

    case PARSER_ASTNODETYPE_FUNC_DECL:
        codegen_func_node(node, scope, allocs, context, mod);
        break;

    case PARSER_ASTNODETYPE_VAR_DECL:
        codegen_var_node(node, scope, context, mod, builder);
        break;

    case PARSER_ASTNODETYPE_RETURN:
        codegen_ret_node(node, scope, context, mod, builder);
        break;

    case PARSER_ASTNODETYPE_CLASS:
        codegen_class_node(node, scope, allocs, context, mod);
        break;

    case PARSER_ASTNODETYPE_NAMESPACE:
        codegen_nmspace_node(node, scope, allocs, context, mod);
        break;

    case PARSER_ASTNODETYPE_EXPR:
        codegen_expr(&node->expr, scope, context, mod, builder);
        break;

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

    print_module(mod);
    verify_module(mod);

    LLVMDisposeModule(mod);
    LLVMContextDispose(context);
}
