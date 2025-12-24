#!/bin/bash
set -e

cd "$LIBRESECT_SRC_DIR"
mkdir -p .build/ && cd .build/
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_C_COMPILER='clang-21' ../ && cmake --build .