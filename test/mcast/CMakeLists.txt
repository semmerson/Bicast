add_executable(Mcast_test Mcast_test.cpp)
target_link_libraries(Mcast_test bicast gtest)
add_test(Mcast_test Mcast_test)
