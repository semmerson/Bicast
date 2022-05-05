include_directories(
        ${CMAKE_SOURCE_DIR}/main
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
        ${CMAKE_SOURCE_DIR}/main/p2p
        ${CMAKE_SOURCE_DIR}/main/mcast
        ${CMAKE_SOURCE_DIR}/main/repository
        ${CMAKE_SOURCE_DIR}/main/node
)

add_executable(Node_test Node_test.cpp)
target_link_libraries(Node_test hycast gtest)
add_test(Node_test Node_test)