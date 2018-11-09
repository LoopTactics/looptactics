project(islutils)
include(ExternalProject)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_CXX_FLAGS "-Wall -pedantic -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -pedantic -O0 -g")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

ExternalProject_Add(isl
     PREFIX external/isl
     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/isl
     BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-isl
     INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl
     UPDATE_COMMAND ./autogen.sh
     PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/external/isl-no-throw-copy-constructor.patch COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/external/isl-expose-all.patch
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-clang=system --with-int=imath-32
     BUILD_COMMAND make
     EXCLUDE_FROM_ALL 1
)

ExternalProject_Add(pet
     DEPENDS isl
     PREFIX external/pet
     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/pet
     BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-pet
     INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-pet
     UPDATE_COMMAND ./autogen.sh
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-isl-prefix=${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl --with-isl=system
     BUILD_COMMAND make
     EXCLUDE_FROM_ALL 1
)

add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

set(isl_LIB "libisl.so")
set(isl_LIBPATH "external/install-isl/lib")
set(isl_INCLUDE "external/install-isl/include")

set(pet_LIB "libpet.so")
set(pet_LIBPATH "external/install-pet/lib/")
set(pet_INCLUDE "external/install-pet/include")

set(boost_LIB "libboost_program_options.so")
set(boost_LIBPATH "external/boost_1_67_0/stage/lib")
set(boost_INCLUDE "external/boost_1_67_0/boost")

link_directories(islutils ${isl_LIBPATH} ${pet_LIBPATH} ${boost_LIBPATH})

add_library(islutils

            islutils/matchers.cc
            islutils/builders.cc
            islutils/pet_wrapper.cc
            islutils/access_patterns.cc)
target_include_directories(islutils PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(islutils PUBLIC ${isl_INCLUDE} ${pet_INCLUDE})
target_include_directories(islutils PUBLIC ${isl_INCLUDE} ${pet_INCLUDE} ${boost_include})
target_link_libraries(islutils ${isl_LIB} ${pet_LIB} ${boost_LIB})

add_subdirectory(test)
add_subdirectory(docs)
