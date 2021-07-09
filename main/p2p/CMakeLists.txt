# Add the library
add_library(p2p OBJECT
        DataSeg.cpp
        Peer.cpp        Peer.h
                        P2pNode.h
        NoticeQueue.cpp NoticeQueue.h
        PeerSet.cpp     PeerSet.h
#       Bookkeeper.cpp  Bookkeeper.h
)
include_directories(.. ../misc ../inet)
