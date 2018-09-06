cmake_minimum_required(VERSION 3.2)

set(TEST_NAMES
    set_maker
    matcher
    builders
    transformer
    access)

add_custom_target(check COMMAND echo "Running all")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inputs/one-dimensional-init.c"
     "${CMAKE_CURRENT_BINARY_DIR}/inputs/one-dimensional-init.c"; COPYONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inputs/stencil.c"
     "${CMAKE_CURRENT_BINARY_DIR}/inputs/stencil.c"; COPYONLY)

foreach(case ${TEST_NAMES})
  add_executable("test_${case}" "test_${case}.cc")
  target_link_libraries("test_${case}" islutils)
  target_link_libraries("test_${case}" gtest_main)

  add_custom_target("check-${case}" COMMAND "test_${case}")
  add_dependencies(check "check-${case}")
endforeach()
