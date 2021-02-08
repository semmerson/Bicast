include_directories(
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
)

add_executable(SockAddr_test SockAddr_test.cpp)
target_link_libraries(SockAddr_test hycast gtest)
add_test(SockAddr_test SockAddr_test)

add_executable(Socket_test Socket_test.cpp)
target_link_libraries(Socket_test hycast gtest pthread)
add_test(Socket_test Socket_test)

add_executable(ReuseAddr_test ReuseAddr_test.cpp)
target_link_libraries(ReuseAddr_test hycast gtest)
add_test(ReuseAddr_test ReuseAddr_test)