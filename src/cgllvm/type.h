#pragma once

#include "cgllvm/llvm_vecs.h"
#include "parser/class.h"
#include "parser/type.h"
#include <llvm-c/Types.h>

LLVMTypeRef CGLLVM_convert_parser_type(const struct Parser_Type *type,
                                       LLVMContextRef context);
char *CGLLVM_named_type_full_name(const struct Parser_TypeNamed *named);

struct CGLLVM_TypeRefVec
CGLLVM_class_to_struct_fields(const struct Parser_Class *src,
                              LLVMContextRef context);
LLVMTypeRef CGLLVM_create_struct(const struct Parser_Class *src,
                                 LLVMContextRef context);
