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


## Valgrind check
```sh
valgrind --suppressions=../valgrind.sup --leak-check=full --show-leak-kinds=all --read-var-info=yes --track-origins=yes --log-file=valgrind-out.txt ./resect-test "/path/to/header.h"
```
