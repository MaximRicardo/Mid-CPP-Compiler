#include "codegen.h"
#include "macros.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>
#include <stdio.h>

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
