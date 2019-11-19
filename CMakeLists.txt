project(islutils)
include(ExternalProject)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_CXX_FLAGS "-Wall -pedantic -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -pedantic -O0 -g")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Arch: ${CMAKE_SYSTEM_PROCESSOR}")

option(VERBOSE "debug" OFF)
if (VERBOSE)
  add_definitions("-DDEBUG")
endif (VERBOSE)

# Isl for haystack, modified to work also with matchers/builders.
ExternalProject_Add(isl-haystack
    PREFIX external/isl-haystack
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/isl-haystack
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-isl-haystack
    INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl-haystack
    UPDATE_COMMAND ./autogen.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-clang=system --with-int=imath-32
    BUILD_COMMAND make -j4
    EXCLUDE_FROM_ALL 1
)

# Pet 
ExternalProject_Add(pet
     DEPENDS isl-haystack
     PREFIX external/pet
     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/pet
     BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-pet
     INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-pet
     UPDATE_COMMAND ./autogen.sh
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-isl-prefix=${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl-haystack --with-isl=system
     BUILD_COMMAND make -j4
     EXCLUDE_FROM_ALL 1
)

add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

set(pet_LIB "libpet.so")
set(pet_LIBPATH "external/install-pet/lib")
set(pet_INCLUDE "external/install-pet/include")

set(isl-haystack_LIB "libisl.so")
set(isl-haystack_LIBPATH "external/install-isl-haystack/lib")
set(isl-haystack_INCLUDE "external/install-isl-haystack/include")

link_directories(islutils ${isl-haystack_LIBPATH} ${pet_LIBPATH} ${barvinok_LIBPATH})

add_library(islutils

            islutils/matchers.cc
            islutils/builders.cc
            islutils/pet_wrapper.cc
            islutils/access_patterns.cc
)

target_include_directories(islutils PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(islutils PUBLIC ${isl-haystack_INCLUDE})
target_include_directories(islutils PUBLIC ${pet_INCLUDE})
target_link_libraries(islutils ${isl-haystack_LIB})
target_link_libraries(islutils ${pet_LIB})

add_executable(main 
  main.cc
)

add_subdirectory(test)
add_subdirectory(docs)
