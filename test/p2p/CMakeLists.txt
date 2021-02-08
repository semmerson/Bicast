include_directories(
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
        ${CMAKE_SOURCE_DIR}/main/protocol
        ${CMAKE_SOURCE_DIR}/main/p2p
        ${CMAKE_SOURCE_DIR}/main/node
)

add_executable(ChunkIdQueue_test ChunkIdQueue_test.cpp)
target_link_libraries(ChunkIdQueue_test hycast gtest)
add_test(ChunkIdQueue_test ChunkIdQueue_test)

add_executable(Peer_test Peer_test.cpp)
target_link_libraries(Peer_test hycast gtest)
add_test(Peer_test Peer_test)

add_executable(PeerFactory_test PeerFactory_test.cpp)
target_link_libraries(PeerFactory_test hycast gtest)
add_test(PeerFactory_test PeerFactory_test)

add_executable(PeerSet_test PeerSet_test.cpp)
target_link_libraries(PeerSet_test hycast gtest)
add_test(PeerSet_test PeerSet_test)

add_executable(P2pMgr_test P2pMgr_test.cpp)
target_link_libraries(P2pMgr_test hycast gtest)
add_test(P2pMgr_test P2pMgr_test)