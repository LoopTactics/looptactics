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
     PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/external/isl-no-throw-copy-constructor.patch
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-clang=system --with-int=imath-32
     BUILD_COMMAND make -j
)

ExternalProject_Add(pet
     DEPENDS isl
     PREFIX external/pet
     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/pet
     BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-pet
     INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-pet
     UPDATE_COMMAND ./autogen.sh
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-isl-prefix=${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl --with-isl=system
     BUILD_COMMAND make -j
)

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()

set(isl_LIB "libisl.so")
set(isl_LIBPATH "external/install-isl/lib")
set(isl_INCLUDE "external/install-isl/include")

set(pet_LIB "libpet.so")
set(pet_LIBPATH "external/install-pet/lib/")
set(pet_INCLUDE "external/install-pet/include")

link_directories(islutils ${isl_LIBPATH} ${pet_LIBPATH})

add_library(islutils

            islutils/matchers.cc
            islutils/parser.cc
            islutils/builders.cc)
target_include_directories(islutils PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(islutils PUBLIC ${isl_INCLUDE} ${pet_INCLUDE})
target_link_libraries(islutils ${isl_LIB} ${pet_LIB})

add_subdirectory(test)
