include_directories(
        ${CMAKE_SOURCE_DIR}/main/proto
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
)

add_executable(Peer_test Peer_test.cpp)
target_link_libraries(Peer_test hycast gtest)
add_test(Peer_test Peer_test)