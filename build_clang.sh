#!/bin/bash

WORK_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
BUILD_DIR=$WORK_DIR/build/llvm/
LLVM_PROJECTS_DIR=$WORK_DIR/llvm
LLVM_DIR=$LLVM_PROJECTS_DIR/llvm
LLVM_INSTALL_DIR=$WORK_DIR/llvm-bin

mkdir -p $BUILD_DIR && cd $BUILD_DIR

CPU_COUNT=$(nproc)
BUILD_THREAD_COUNT=$(($CPU_COUNT/4))
BUILD_THREAD_COUNT=$(($BUILD_THREAD_COUNT > 0 ? $BUILD_THREAD_COUNT : 1))

# gold linker reduces memory consumption during linking
cmake -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL_DIR \
      -DLLVM_USE_LINKER=gold \
      -DLLVM_ENABLE_PROJECTS="clang;" \
      -DLLVM_TARGETS_TO_BUILD='X86;' \
      -DLIBCLANG_BUILD_STATIC=ON \
      -DBUILD_SHARED_LIBS=OFF \
      $LLVM_DIR \
    && cmake --build $BUILD_DIR --target install --config Release --parallel $BUILD_THREAD_COUNT
