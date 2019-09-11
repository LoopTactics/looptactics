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

# Qt libraries
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
include_directories(${Qt5Core_INCLUDE_DIRS})
ADD_DEFINITIONS(-DQT_NO_KEYWORDS)

# Boost
find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Isl for matchers/builders
ExternalProject_Add(isl
     PREFIX external/isl
     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/isl
     BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-isl
     INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl
     UPDATE_COMMAND ./autogen.sh
     PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/external/isl-no-throw-copy-constructor.patch COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/external/isl-expose-all.patch
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-clang=system --with-int=imath-32
     BUILD_COMMAND make -j4
     EXCLUDE_FROM_ALL 1
)

# Isl for haystack
ExternalProject_Add(isl_haystack
    PREFIX external/isl_haystack
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/isl_haystack
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-isl_haystack
    INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl_haystack
    UPDATE_COMMAND ./autogen.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-clang=system --with-int=imath-32
    BUILD_COMMAND make -j4
    EXCLUDE_FROM_ALL 1
)

# Pet 
ExternalProject_Add(pet
     DEPENDS isl isl_haystack
     PREFIX external/pet
     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/pet
     BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-pet
     INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-pet
     UPDATE_COMMAND ./autogen.sh
     CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-isl-prefix=${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl --with-isl=system
     BUILD_COMMAND make -j4
     EXCLUDE_FROM_ALL 1
)

# Barvinok for haystack
ExternalProject_Add(barvinok
    DEPENDS pet
    PREFIX external/barvinok
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/barvinok
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-barvinok
    INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-barvinok
    UPDATE_COMMAND ./autogen.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared-barvinok 
    BUILD_COMMAND make -j4
    EXCLUDE_FROM_ALL 1
)

add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

set(isl_LIB "libisl.so")
set(isl_LIBPATH "external/install-isl/lib")
set(isl_INCLUDE "external/install-isl/include")

set(pet_LIB "libpet.so")
set(pet_LIBPATH "external/install-pet/lib")
set(pet_INCLUDE "external/install-pet/include")

set(barvinok_LIB "libbarvinok.so")
set(barvinok_LIBPATH "external/install-barvinok/lib")
set(barvinok_INCLUDE "external/install-barvinok/include")

set(isl_haystack_LIB "libisl.so")
set(isl_haystack_LIBPATH "external/install-isl_haystack/lib")
set(isl_haystack_INCLUDE "external/install-isl_haystack/include")

link_directories(islutils ${isl_LIBPATH} ${pet_LIBPATH} ${barvinok_LIBPATH} ${isl_haystack_LIBPATH})

add_library(islutils

            islutils/matchers.cc
            islutils/builders.cc
            islutils/pet_wrapper.cc
            islutils/util.cc
            islutils/access_patterns.cc
            islutils/parser.cc
            islutils/loop_opt.cc
            haystack/Access.cpp
            haystack/HayStack.cpp
            haystack/Program.cpp
            haystack/Timer.cpp
            haystack/isl-helpers.cpp
)
target_include_directories(islutils PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(islutils PUBLIC ${isl_INCLUDE} ${pet_INCLUDE})
target_include_directories(islutils PUBLIC ${barvinok_INCLUDE} ${isl_haystack_INCLUDE})
target_link_libraries(islutils ${isl_LIB} ${pet_LIB} ${barvinok_LIB} ${isl_haystack_LIB})

#add_executable(main 
#  main.cc
#  islutils/highlighter.cc
#  islutils/mainwindow.cc
#  islutils/tuner_thread.cc
#  )

add_executable(main
    haystack/main.cpp
)

target_link_libraries(main islutils)
target_link_libraries(main Qt5::Widgets)
target_link_libraries(main ${Boost_LIBRARIES})

add_subdirectory(test)
add_subdirectory(docs)
