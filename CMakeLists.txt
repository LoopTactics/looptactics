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

set(VERBOSE_LEVEL OFF CACHE STRING "debug level")
set_property(CACHE VERBOSE_LEVEL PROPERTY STRINGS OFF 1 2 3)

if (VERBOSE_LEVEL STREQUAL "1")
  add_definitions("-DLEVEL_ONE")
endif (VERBOSE_LEVEL STREQUAL "1")

if (VERBOSE_LEVEL STREQUAL "2")
  add_definitions("-DLEVEL_TWO")
endif (VERBOSE_LEVEL STREQUAL "2")

if (VERBOSE_LEVEL STREQUAL "3")
  add_definitions("-DLEVEL_THREE")
endif (VERBOSE_LEVEL STREQUAL "3")

# Qt libraries
#set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#find_package(Qt5 COMPONENTS Widgets REQUIRED)
#include_directories(${Qt5Core_INCLUDE_DIRS})
#ADD_DEFINITIONS(-DQT_NO_KEYWORDS)

# LLVM used to get cache configuration via TTI.
#find_package(LLVM REQUIRED CONFIG)
#message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
#message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
#include_directories(${LLVM_INCLUDE_DIRS})
#add_definitions(${LLVM_DEFINITIONS})

# Boost
# https://stackoverflow.com/questions/9723793/undefined-reference-to-boostsystemsystem-category-when-compiling
#add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)

# prevent error 
# "You must build your code with position independent code if Qt was built with..
# https://github.com/tora-tool/tora/blob/master/CMakeLists.txt
#if (Qt5_POSITION_INDEPENDENT_CODE)
#  SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
#endif()


# Boost
#find_package(Boost COMPONENTS program_options filesystem system REQUIRED)
#include_directories(${Boost_INCLUDE_DIRS})

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

# Barvinok for haystack
ExternalProject_Add(barvinok
    DEPENDS pet
    PREFIX external/barvinok
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/barvinok
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/build-barvinok
    INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/install-barvinok
    UPDATE_COMMAND ./autogen.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared-barvinok #--with-isl-prefix=${CMAKE_CURRENT_SOURCE_DIR}/external/install-isl-haystack --with-isl=system 
    BUILD_COMMAND make -j4
    EXCLUDE_FROM_ALL 1
)

add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

set(pet_LIB "libpet.so")
set(pet_LIBPATH "external/install-pet/lib")
set(pet_INCLUDE "external/install-pet/include")

set(barvinok_LIB "libbarvinok.so")
set(barvinok_LIBPATH "external/install-barvinok/lib")
set(barvinok_INCLUDE "external/install-barvinok/include")

set(isl-haystack_LIB "libisl.so")
set(isl-haystack_LIBPATH "external/install-isl-haystack/lib")
set(isl-haystack_INCLUDE "external/install-isl-haystack/include")

#link_directories(islutils ${pet_LIBPATH} ${barvinok_LIBPATH} ${isl-haystack_LIBPATH})
link_directories(islutils ${isl-haystack_LIBPATH} ${pet_LIBPATH} ${barvinok_LIBPATH})

add_library(islutils

            islutils/matchers.cc
            islutils/builders.cc
            islutils/pet_wrapper.cc
            #islutils/util.cc
            islutils/access_patterns.cc
            #islutils/haystack_runner.cc
            islutils/parser.cc
            #islutils/loop_opt.cc
            haystack/Access.cpp
            haystack/HayStack.cpp
            haystack/Program.cpp
            haystack/Timer.cpp
            haystack/isl-helpers.cpp
)

# Find the libraries that correspond to the LLVM components
# that we wish to use
#llvm_map_components_to_libnames(llvm_libs core nativecodegen ExecutionEngine)

target_include_directories(islutils PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(islutils PUBLIC ${isl-haystack_INCLUDE})
target_include_directories(islutils PUBLIC ${pet_INCLUDE})
target_include_directories(islutils PUBLIC ${barvinok_INCLUDE})
target_link_libraries(islutils ${isl-haystack_LIB})
target_link_libraries(islutils ${pet_LIB})
target_link_libraries(islutils ${barvinok_LIB})
#target_link_libraries(islutils ${llvm_libs})
#target_link_libraries(islutils ${Boost_LIBRARIES})

#add_executable(main 
#  main.cc
#  islutils/highlighter.cc
#  islutils/mainwindow.cc
#  islutils/tuner_thread.cc
#  )

#add_executable(main
#    haystack/main.cpp
#)

#target_link_libraries(main islutils)
#target_link_libraries(main Qt5::Widgets)
#target_link_libraries(main ${Boost_LIBRARIES})

add_subdirectory(test)
add_subdirectory(docs)
