add_executable(regex_test regex_test.cpp)
add_test(regex_test regex_test)

add_executable(Action_test Action_test.cpp)
target_link_libraries(Action_test hycast gtest)
add_test(Action_test Action_test)

add_executable(Disposer_test Disposer_test.cpp)
target_link_libraries(Disposer_test hycast gtest)
set(DISPOSER_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Disposer.yaml")
# The following doesn't work: the process doesn't see the argument
add_test(Disposer_test Disposer_test ${DISPOSER_CONFIG_FILE})