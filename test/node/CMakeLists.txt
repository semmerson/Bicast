add_executable(Node_test Node_test.cpp)
target_link_libraries(Node_test hycast gtest)
add_test(Node_test Node_test)