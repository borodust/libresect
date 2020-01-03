cmake_minimum_required(VERSION 3.8)
project(resect)

set(CMAKE_C_STANDARD 99)

add_library(resect SHARED resect.c resect.h)
target_link_libraries(resect clang)

add_executable(resect-test test.c)
target_link_libraries(resect-test resect)
