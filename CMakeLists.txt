cmake_minimum_required(VERSION 3.4.3)
project(mcppc)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM 20.1.8 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# include(llvm.cmake)

file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(mcppc "${Sources}")

# add_compile_options(-Wall -Wextra -Wpedantic -std=c23 -fno-strict-aliasing)

#For debugging
target_compile_options(mcppc PRIVATE -Wall -Wextra -Wpedantic -std=c23 -ggdb3 -fsanitize=address -fno-strict-aliasing)
target_link_options(mcppc PRIVATE -fsanitize=address)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs core analysis target x86info x86targetmca x86disassembler x86desc x86codegen x86asmparser)

target_link_libraries(mcppc PRIVATE ${llvm_libs})
target_include_directories(mcppc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# target_include_directories(mcppc SYSTEM PRIVATE "/usr/include/llvm-c-20" "/usr/include/llvm-20")
target_compile_definitions(mcppc PRIVATE GEN_DYNARRAY_DEFAULT_SIZE_TYPE=isize_t GEN_BUMPALLOC_DEFAULT_SIZE_TYPE=isize_t)
