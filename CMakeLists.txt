cmake_minimum_required(VERSION 3.18)
project(resect C)

set(LIBCLANG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libclang-bundle/")
set(LIBCLANG_INCLUDE_DIRS "${LIBCLANG_DIR}/include/")
set(LIBCLANG_LINK_DIRS "${LIBCLANG_DIR}/lib/")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
#
# PCRE
#
set(PCRE2_STATIC ON)
set(PCRE2_STATIC_RUNTIME ON)
set(PCRE2_BUILD_PCRE2_8 ON)
set(PCRE2_BUILD_TESTS OFF)
set(PCRE2_BUILD_PCRE2GREP OFF)

add_subdirectory("${THIRD_PARTY_DIR}/pcre2-10.37/")

set_target_properties(pcre2-8 PROPERTIES
        CMAKE_POSITION_INDEPENDENT_CODE ON)
#
# RESECT
#
add_library(resect SHARED
        resect.h
        src/parser.c
        src/decl.c
        src/type.c
        src/context.c
        src/util.c
        src/filtering.c
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

if (APPLE OR UNIX)

    set_target_properties(resect_clang_static PROPERTIES
        IMPORTED_LOCATION "${LIBCLANG_LINK_DIRS}/libclang_bundled.a")

#    target_link_options(resect PUBLIC "-fuse-ld=gold")
    target_link_libraries(resect
        stdc++ m dl pthread z)
elseif(WIN32)
    set_target_properties(resect_clang_static PROPERTIES
        IMPORTED_LOCATION "${LIBCLANG_LINK_DIRS}/clang_static_bundled.lib")
    target_link_libraries(resect
        version.lib)
endif()

target_link_libraries(resect
    resect_clang_static pcre2-8)

add_executable(resect-test test/test.c)
target_link_libraries(resect-test PUBLIC resect)
