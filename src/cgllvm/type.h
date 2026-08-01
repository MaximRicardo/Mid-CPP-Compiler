#pragma once

#include "cgllvm/llvm_vecs.h"
#include "ints.h"
#include "parser/class.h"
#include "parser/type.h"
#include "sema/ident.h"
#include <llvm-c/Types.h>

LLVMTypeRef MidLLVM_convert_parser_type(const struct MidParser_Type *type,
                                        LLVMContextRef context,
                                        bool ref_is_ptr);
char *MidLLVM_named_type_full_name(const struct MidSema_IdentPtr *named);

struct MidLLVM_TypeRefVec
MidLLVM_class_to_struct_fields(const struct MidParser_Class *src,
                               LLVMContextRef context);
LLVMTypeRef MidLLVM_create_struct(const struct MidParser_Class *src,
                                  LLVMContextRef context);
mid_isize
MidLLVM_class_field_to_struct_field_idx(const struct MidParser_Class *src,
                                        const char *name);
