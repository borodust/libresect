cmake_minimum_required(VERSION 3.16)
project(resect C)

find_package(Clang 21.1.0 REQUIRED CONFIG)
include_directories(${CLANG_INCLUDE_DIRS})

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
#
# PCRE
#
set(PCRE2_STATIC ON CACHE BOOL "" FORCE)
set(PCRE2_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2_8 ON CACHE BOOL "" FORCE)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBZ OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBBZ2 OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBREADLINE OFF CACHE BOOL "" FORCE)
set(PCRE2_STATIC_PIC ON)

add_subdirectory("${THIRD_PARTY_DIR}/pcre2-10.47/")

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
        CMAKE_C_STANDARD 99
        CMAKE_POSITION_INDEPENDENT_CODE ON
        CMAKE_INCLUDE_CURRENT_DIR ON
        LINK_FLAGS_RELEASE -s
        LINK_WHAT_YOU_USE TRUE)

target_include_directories(resect
        SYSTEM BEFORE PUBLIC "${CMAKE_BINARY_DIR}/third-party/pcre2-10.47/")

target_link_directories(resect
        PUBLIC "${LIBCLANG_LINK_DIRS}")

if (WIN32)
    target_link_libraries(resect PRIVATE
            version.lib)
endif ()

target_link_libraries(resect PRIVATE
        libclang
        pcre2-8)

add_executable(resect-test test/test.c)
target_link_libraries(resect-test PUBLIC resect)
