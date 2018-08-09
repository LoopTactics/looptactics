Modern C++ utility library for isl.

## Build instructions

```sh
git submodule update --init --recursive
cd external/isl
./autogen.sh
./configure.sh
make -j$(nproc)
cd ../..
mkdir build
cd build
cmake ..
make -j$(nproc)
```
