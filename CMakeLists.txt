project(islutils)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_CXX_FLAGS "-Wall -pedantic -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -pedantic -O0 -g")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(isl_LIB isl HINTS "external/isl/.libs" NO_DEFAULT_PATH)
find_path(isl_INCLUDE "isl/cpp.h" HINTS "external/isl/include" NO_DEFAULT_PATH)

add_library(islutils islutils/matchers.cc)
target_include_directories(islutils PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(islutils PUBLIC ${isl_INCLUDE})
target_link_libraries(islutils ${isl_LIB})

add_subdirectory(test)
