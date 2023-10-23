add_executable(IfacePeer IfacePeer.cpp)

add_executable(shared_ptr_test shared_ptr_test.cpp)

add_executable(EnumArgConversion EnumArgConversion.cpp)

add_executable(Peer_test Peer_test.cpp)
target_link_libraries(Peer_test bicast gtest)
add_test(Peer_test Peer_test)

##add_executable(NoticeQueue_test NoticeQueue_test.cpp)
##target_link_libraries(NoticeQueue_test bicast gtest)
##add_test(NoticeQueue_test NoticeQueue_test)

##add_executable(PeerSet_test PeerSet_test.cpp)
##target_link_libraries(PeerSet_test bicast gtest)
##add_test(PeerSet_test PeerSet_test)

add_executable(Bookkeeper_test Bookkeeper_test.cpp)
target_link_libraries(Bookkeeper_test bicast gtest)
add_test(Bookkeeper_test Bookkeeper_test)

add_executable(P2pMgr_test P2pMgr_test.cpp)
target_link_libraries(P2pMgr_test bicast gtest)
add_test(P2pMgr_test P2pMgr_test)
