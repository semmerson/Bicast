add_executable(ReuseAddr_test ReuseAddr_test.cpp)
target_link_libraries(ReuseAddr_test hycast gtest)
add_test(ReuseAddr_test ReuseAddr_test)

add_executable(connect_test connect_test.c)
target_link_libraries(connect_test pthread)
add_test(connect_test connect_test)

add_executable(NonBlockingConn_test NonBlockingConn_test.c)
target_link_libraries(NonBlockingConn_test pthread)
add_test(NonBlockingConn_test connect_test)

add_executable(SockAddr_test SockAddr_test.cpp)
target_link_libraries(SockAddr_test hycast gtest)
add_test(SockAddr_test SockAddr_test)

add_executable(socket_test socket_test.cpp)
target_link_libraries(socket_test hycast gtest pthread)
add_test(socket_test socket_test)

add_executable(Socket_test Socket_test.cpp)
target_link_libraries(Socket_test hycast gtest pthread)
add_test(Socket_test Socket_test)

add_executable(Xprt_test Xprt_test.cpp)
target_link_libraries(Xprt_test hycast gtest pthread)
add_test(Xprt_test Xprt_test)

add_executable(Connect_test Connect_test.cpp)
target_link_libraries(Connect_test hycast pthread)
add_test(Connect_test Connect_test)

add_executable(SsmRecvTest SsmRecvTest.c)
add_executable(SsmSendTest SsmSendTest.c)