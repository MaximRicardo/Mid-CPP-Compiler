cmake_minimum_required(VERSION 3.4.3)

project(mcppc)

add_library(mcppc-lib apfloat.c apint.c cgllvm/allocator.c cgllvm/codegen.c cgllvm/ident.c cgllvm/name_mangle.c cgllvm/scope.c cgllvm/type.c cmd.c diag.c dynstr.c lexer/token.c lexer/tokenize.c literal.c parser/allocator.c parser/ast.c parser/ast_log.c parser/astvec.c parser/class.c parser/decl.c parser/enum.c parser/expr.c parser/expr_type.c parser/find_twin.c parser/func_decl.c parser/namespace.c parser/return.c parser/scope.c parser/template.c parser/type.c parser/var_decl.c position.c sema/class.c sema/class_lit.c sema/expr.c sema/expr_eval.c sema/func.c sema/ident.c sema/lookup.c sema/scope.c sema/template.c sema/type.c sema/typecheck.c sema/var.c sort.c types.c utf8.c)

add_executable(mcppc main.c)

add_dependencies(mcppc mcppc-lib)

# add_compile_options(-Wall -Wextra -Wpedantic -std=c23 -fno-strict-aliasing)

#For debugging
target_compile_options(mcppc-lib PUBLIC -Wall -Wextra -Wpedantic -std=c23 -ggdb3 -fsanitize=address -fno-strict-aliasing)
target_link_options(mcppc-lib PUBLIC -fsanitize=address)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs core analysis target x86info x86targetmca x86disassembler x86desc x86codegen x86asmparser)

target_link_libraries(mcppc PRIVATE mcppc-lib)
target_link_libraries(mcppc-lib PUBLIC ${llvm_libs})
target_include_directories(mcppc-lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
# target_include_directories(mcppc SYSTEM PRIVATE "/usr/include/llvm-c-20" "/usr/include/llvm-20")
target_compile_definitions(mcppc-lib PUBLIC
    MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE=mid_isize
    MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE=mid_isize
    #MIDPAR_DEBUG_LOG_NODES    # enable to print basic info about AST nodes
    #MIDPAR_DEBUG_LOG_CALLS     # enable to print basic info about func calls
)
