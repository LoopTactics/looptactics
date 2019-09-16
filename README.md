Modern C++ utility library for isl.

## Build instructions

Dependencies are fetched via git submodules.  Since they don't use cmake, we
rely on cmake external projects to run the autoconf/configure/make toolchain.
This is done by `make pet` below, which only has to run in the first build or
after submodule update.

```sh
git submodule update --init --recursive
cmake .
make barvinok
make
```

We use Haystack (https://github.com/spcl/haystack)
Haystack is an analytical cache model that given a program computes the number of cache misses. The tool aims at providing the programmer with a better intuition of the memory access costs that on todays machines increasingly dominate the overall program execution time. The tool counts the cache misses symbolically and thus neither executes the program nor enumerates all memory accesses explicitly which makes the model runtime problem size independent. The tool models fully associative caches with LRU replacement policy.

The paper "A Fast Analytical Model of Fully Associative Caches" (Tobias Gysi, Tobias Grosser, Laurin Brandner, and Torsten Hoefler) provides further implementation details. The software was developed by SPCL (ETH Zurich).


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

If you use this tool please cite:
```
@article{zinenko2018declarative,
  title={Declarative Transformations in the Polyhedral Model},
  author={Zinenko, Oleksandr and Chelini, Lorenzo and Grosser, Tobias},
  year={2018}
}
```
