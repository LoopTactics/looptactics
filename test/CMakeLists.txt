cmake_minimum_required(VERSION 3.2)

set(TEST_NAMES
    set_maker
    matcher
    builders
    transformer
    access)

set(TEST_INPUTS
    3mm.c
    2mm.c
    gemm.c
    one-dimensional-init.c
    1mm_fused.c
    nested.c
    strided_domain.c
    strided_domain_with_coefficients.c
    strided_domain_multi_dimensions.c
    1mmWithoutInitStmt.c
    stencilMix.c)

add_custom_target(check COMMAND echo "Running all")

foreach(file ${TEST_INPUTS})
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inputs/${file}"
       "${CMAKE_CURRENT_BINARY_DIR}/inputs/${file}" COPYONLY)
endforeach()

foreach(case ${TEST_NAMES})
  add_executable("test_${case}" "test_${case}.cc")
  target_link_libraries("test_${case}" islutils)
  target_link_libraries("test_${case}" gtest_main)

  add_custom_target("check-${case}" COMMAND "test_${case}")
  add_dependencies(check "check-${case}")
endforeach()

