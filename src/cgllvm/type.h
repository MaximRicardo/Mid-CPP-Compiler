#pragma once

#include "cgllvm/llvm_vecs.h"
#include "ints.h"
#include "parser/class.h"
#include "parser/type.h"
#include "sema/ident.h"
#include <llvm-c/Types.h>

LLVMTypeRef midllvm_convert_parser_type(const struct midpar_Type *type,
                                        LLVMContextRef context,
                                        bool ref_is_ptr);
char *midllvm_named_type_full_name(const struct midsema_IdentPtr *named);

struct midllvm_TypeRefVec
midllvm_class_to_struct_fields(const struct midpar_Class *src,
                               LLVMContextRef context);
LLVMTypeRef midllvm_create_struct(const struct midpar_Class *src,
                                  LLVMContextRef context);
mid_isize
midllvm_class_field_to_struct_field_idx(const struct midpar_Class *src,
                                        const char *name);
