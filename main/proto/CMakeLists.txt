# Add the library
add_library(proto OBJECT
        HycastProto.cpp HycastProto.h
        DataSeg.cpp
        P2pConn.cpp     P2pConn.h
        Peer.cpp        Peer.h
                        P2pNode.h
        NoticeQueue.cpp NoticeQueue.h
        PeerSet.cpp     PeerSet.h
        Bookkeeper.cpp  Bookkeeper.h
)
include_directories(../misc ../inet)
