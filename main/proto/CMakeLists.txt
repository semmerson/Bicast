# Add the library
add_library(proto OBJECT
        HycastProto.cpp HycastProto.h
        DataSeg.cpp
        Peer.cpp        Peer.h
                        P2pNode.h
        NoticeQueue.cpp NoticeQueue.h
        PeerSet.cpp     PeerSet.h
        Bookkeeper.cpp  Bookkeeper.h
)
include_directories(../misc ../inet)
