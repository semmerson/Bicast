add_executable(Mcast_test Mcast_test.cpp)
target_link_libraries(Mcast_test hycast gtest)
add_test(Mcast_test Mcast_test)