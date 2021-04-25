cmake_minimum_required(VERSION 3.18)
project(resect)

set(LLVM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llvm-bin/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM libs: ${LLVM_AVAILABLE_LIBS}")

find_package(Clang REQUIRED CONFIG
        PATHS "${CMAKE_CURRENT_SOURCE_DIR}/llvm-bin/lib/cmake/clang"
        NO_DEFAULT_PATH)
message(STATUS "Clang found in: ${CLANG_CMAKE_DIR}")

set(CMAKE_C_STANDARD 99)

include_directories(BEFORE SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
link_directories(BEFORE SYSTEM ${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(resect SHARED
        resect.h
        src/parser.c
        src/decl.c
        src/type.c
        src/context.c
        src/util.c
        src/resect_private.h)

set_target_properties(resect PROPERTIES
        LINK_FLAGS_RELEASE -s
        LINK_WHAT_YOU_USE TRUE)

target_link_libraries(resect libclang_static)

add_executable(resect-test test/test.c)
target_link_libraries(resect-test resect)
