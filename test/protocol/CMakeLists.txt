include_directories(
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
        ${CMAKE_SOURCE_DIR}/main/protocol
        ${CMAKE_SOURCE_DIR}/main/node
)

add_executable(PeerProto_test PeerProto_test.cpp)
target_link_libraries(PeerProto_test hycast gtest)
add_test(PeerProto_test PeerProto_test)

add_executable(McastProto_test McastProto_test.cpp)
target_link_libraries(McastProto_test hycast gtest)
add_test(McastProto_test McastProto_test)