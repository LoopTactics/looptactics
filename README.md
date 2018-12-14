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
export CPLUS_INCLUDE_PATH= /whereLLVMisLocated/llvm_build/tools/clang/include/
export CPLUS_INCLUDE_PATH=/whereLLVMisLocated/llvm/tools/clang/include/:$CPLUS_INCLUDE_PATH
```

Brief explanation:
This is the matchers/builder framework to be used as a source-to-source compiler.
