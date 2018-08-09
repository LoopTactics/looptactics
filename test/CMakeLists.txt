cmake_minimum_required(VERSION 3.2)

add_executable(test_set_maker test_set_maker.cc)
target_link_libraries(test_set_maker islutils)

add_executable(test_matcher test_matcher.cc)
target_link_libraries(test_matcher islutils)
target_link_libraries(test_matcher gtest_main)

add_test(NAME test_matcher COMMAND test)
