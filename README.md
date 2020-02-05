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


Tested in:
```
clang version 9.0.0
Target: x86_64-pc-linux-gnu
Thread model: posix
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

Contributors:

Alex Zinenko (https://ozinenko.com/)

Lorenzo Chelini (l.chelini@tue.nl / l.chelini@icloud.com)

Tobias Grosser (https://www.grosser.es/)


If you use this tool please cite:
```
@article{zinenko2018declarative,
  title={Declarative Transformations in the Polyhedral Model},
  author={Zinenko, Oleksandr and Chelini, Lorenzo and Grosser, Tobias},
  year={2018}
}

@article{10.1145/3372266,
author = {Chelini, Lorenzo and Zinenko, Oleksandr and Grosser, Tobias and Corporaal, Henk},
title = {Declarative Loop Tactics for Domain-Specific Optimization},
year = {2019},
issue_date = {January 2020},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
volume = {16},
number = {4},
issn = {1544-3566},
url = {https://doi.org/10.1145/3372266},
doi = {10.1145/3372266},
journal = {ACM Trans. Archit. Code Optim.},
month = dec,
articleno = {Article 55},
numpages = {25},
keywords = {declarative loop optimizations, Loop tactics, polyhedral model}
}
```

More information:
Write to l.chelini@tue.nl / l.chelini@icloud.com
