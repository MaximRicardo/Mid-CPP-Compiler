cmake_minimum_required(VERSION 3.12)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(mcppc)

# add_compile_options(-Wall -Wextra -Wpedantic -std=c23 -fno-strict-aliasing)

#For debugging
 add_compile_options(-Wall -Wextra -Wpedantic -std=c23 -ggdb3 -fsanitize=address -fno-strict-aliasing)
 add_link_options(-fsanitize=address)

file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(mcppc "${Sources}")

target_link_libraries(mcppc -lm)
target_include_directories(mcppc
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(mcppc PRIVATE GEN_DYNARRAY_DEFAULT_SIZE_TYPE=isize_t)
