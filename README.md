Modern C++ utility library for isl.

## Build instructions

Dependencies are fetched via git submodules.  Since they don't use cmake, we
rely on cmake external projects to run the autoconf/configure/make toolchain.
This is done by `make pet` below, which only has to run in the first build or
after submodule update.

```sh
git submodule update --init --recursive
cmake .
make pet
make
```

If it does not find clang headers try:
```sh
PATH=/whereLLVMisInstalled/bin
LD_LIBRARY_PATH=/whereLLVMisInstalled/lib
CPLUS_INCLUDE_PATH=/whereLLVMisInstalled/include
```

Brief explanation:
This is the matchers/builder framework to be used as a source-to-source compiler.

Issues:
```
Not working with Clang/LLVM 9.0
Not working with CLang/LLVM 8.0
The reason is that we patch a particular version of ISL that works with Clang/LLVM 6.0
```

Tested in:
```
clang version 6.0.0-1ubuntu2 (tags/RELEASE_600/final)
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
```

```
Distributor ID:	Ubuntu
Description:	Ubuntu 18.04.2 LTS
Release:	18.04
Codename:	bionic
```

```
gcc (Ubuntu 7.3.0-27ubuntu1~18.04) 7.3.0
Copyright (C) 2017 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

```
TODO:
1. Tuner
2. Array packing and dlt.
3. Compute the flops using pet
```
