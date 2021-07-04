#!/bin/bash

WORK_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
BUILD_DIR=$WORK_DIR/build/llvm

LLVM_PROJECTS_DIR=$WORK_DIR/llvm
LLVM_DIR=$LLVM_PROJECTS_DIR/llvm
CLANG_DIR=$LLVM_PROJECTS_DIR/clang
LLVM_INSTALL_DIR=$WORK_DIR/llvm-bin

LIBCLANG_STATIC_BUILD=$WORK_DIR/libclang-static-build
LIBCLANG_STATIC_BUILD_DIR=$WORK_DIR/build/libclang-static
LIBCLANG_STATIC_INSTALL_DIR=$WORK_DIR/libclang-bundle

if [[ -z "$BUILD_THREAD_COUNT" ]]; then
    CPU_COUNT=$(nproc --all)
    BUILD_THREAD_COUNT=$(($CPU_COUNT/4))
    BUILD_THREAD_COUNT=$(($BUILD_THREAD_COUNT > 0 ? $BUILD_THREAD_COUNT : 1))
fi

if [[ -z "$BUILD_TYPE" ]]; then
    BUILD_TYPE="MinSizeRel"
fi

mkdir -p $BUILD_DIR && cd $BUILD_DIR

# gold linker reduces memory consumption during linking
cmake -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL_DIR \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DLLVM_USE_LINKER=gold \
      -DLLVM_ENABLE_PROJECTS="clang;" \
      -DLLVM_TARGETS_TO_BUILD='X86;AArch64;' \
      -DLIBCLANG_BUILD_STATIC=ON \
      -DBUILD_SHARED_LIBS=OFF \
      $LLVM_DIR \
    && cmake --build $BUILD_DIR --target install --config "$BUILD_TYPE" --parallel $BUILD_THREAD_COUNT


mkdir -p $LIBCLANG_STATIC_BUILD_DIR && cd $LIBCLANG_STATIC_BUILD_DIR

cmake -DCMAKE_INSTALL_PREFIX=$LIBCLANG_STATIC_INSTALL_DIR \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DBUILD_SHARED_LIBS=OFF \
      -DLIBCLANG_SOURCES_DIR=$CLANG_DIR \
      -DLIBCLANG_PREBUILT_DIR=$LLVM_INSTALL_DIR \
      $LIBCLANG_STATIC_BUILD \
    && cmake --build $LIBCLANG_STATIC_BUILD_DIR --target install --config "$BUILD_TYPE" --parallel $BUILD_THREAD_COUNT
