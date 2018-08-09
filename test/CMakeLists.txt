cmake_minimum_required(VERSION 3.2)

add_executable(test_set_maker test_set_maker.cc)
target_link_libraries(test_set_maker islutils)
target_link_libraries(test_set_maker gtest_main)

add_executable(test_matcher test_matcher.cc)
target_link_libraries(test_matcher islutils)

add_executable(test_builders test_builders.cc)
target_link_libraries(test_builders islutils)

target_link_libraries(test_matcher gtest_main)

add_custom_target(check COMMAND echo "Running all")

add_custom_target(check-matchers  COMMAND test_matcher)
add_dependencies(check check-matchers)

add_custom_target(check-set-maker COMMAND test_set_maker)
add_dependencies(check check-set-maker)

add_custom_target(check-builders COMMAND test_builders)
add_dependencies(check check-builders)
