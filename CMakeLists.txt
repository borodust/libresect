cmake_minimum_required(VERSION 3.18)
project(resect)

set(LIBCLANG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libclang-bundle/")
set(LIBCLANG_INCLUDE_DIRS "${LIBCLANG_DIR}/include/")
set(LIBCLANG_LINK_DIRS "${LIBCLANG_DIR}/lib/")

add_library(resect SHARED
        resect.h
        src/parser.c
        src/decl.c
        src/type.c
        src/context.c
        src/util.c
        src/resect_private.h)


set_target_properties(resect PROPERTIES
    CMAKE_C_STANDARD 99)
set_target_properties(resect PROPERTIES
    CMAKE_POSITION_INDEPENDENT_CODE ON)

set_target_properties(resect PROPERTIES
        LINK_FLAGS_RELEASE -s
        LINK_WHAT_YOU_USE TRUE)

target_include_directories(resect
  SYSTEM BEFORE PUBLIC "${LIBCLANG_INCLUDE_DIRS}")

target_link_directories(resect
  PUBLIC "${LIBCLANG_LINK_DIRS}")

add_library(resect_clang_static STATIC IMPORTED)

if (APPLE OR LINUX)
    set_target_properties(resect_clang_static PROPERTIES
        IMPORTED_LOCATION "${LIBCLANG_LINK_DIRS}/libclang_bundled.a")
    target_link_libraries(resect
        stdc++ m dl pthread z)
elseif(WIN32)
    set_target_properties(resect_clang_static PROPERTIES
        IMPORTED_LOCATION "${LIBCLANG_LINK_DIRS}/clang_static_bundled.lib")
    target_link_libraries(resect
        version.lib)
endif()

target_link_libraries(resect
    resect_clang_static)

add_executable(resect-test test/test.c)
target_link_libraries(resect-test PUBLIC resect)
