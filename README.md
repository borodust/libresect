# LIBRESECT

This is a tiny Pure C™ wrapper over `libclang` with simplified ABI tailored for
extracting information from C/C++ headers to create bindings to C/C++ libraries.

The goal is to return binary-compatible metadata that can be used to invoke
C/C++ routines from statically or dynamically linked libraries. There's no
guarantee this library would keep all types intact as they specified in a
header. They might be converted to their canonical representations in some
cases.

## ВНИМАНИЕ
It's a work-in-progress library. Neither binary-level nor source-level
compatibility is guaranteed yet.

## Building
Prepare your disk and RAM - we need to build a custom LLVM dist until I push required `libclang` additions into upstream.


#### Windows
Prerequisites
```
choco install -y ninja git cmake gnuwin python3
pip3 install psutil
```
For x64 arch you will need to use this powershell target
```powershell
# adapt to your version of VS
%SystemRoot%\system32\WindowsPowerShell\v1.0\powershell.exe -noe -c "&{Import-Module """C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"""; Enter-VsDevShell 0c759dbc -DevCmdArguments '-arch=x64'}"
```

#### LLVM

```sh
# getting LLVM sources
git submodule update --init --recursive
# building LLVM and static libclang bundle, going for a walk
./build_clang.sh
```

#### libresect

Then we can build `resect` using usual cmake magic:
```sh
mkdir -p build/resect/ && cd build/resect/
cmake -DCMAKE_BUILD_TYPE=Release ../../ && cmake --build .
```

## Valgrind check
```sh
valgrind --suppressions=../valgrind.sup --leak-check=full --show-leak-kinds=all --read-var-info=yes --track-origins=yes --log-file=valgrind-out.txt ./resect-test "/path/to/header.h"
```
