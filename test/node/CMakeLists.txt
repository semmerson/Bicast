add_executable(Node_test Node_test.cpp)
target_link_libraries(Node_test bicast gtest)
add_test(Node_test Node_test)
