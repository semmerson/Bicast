# Add the library
add_library(p2p OBJECT
        DataSeg.cpp
        Peer.cpp          Peer.h
                          P2pNode.h
        NoticeArray.cpp   NoticeArray.h
        PeerSet.cpp       PeerSet.h
        Bookkeeper.cpp    Bookkeeper.h
        PeerSrvrAddrs.cpp PeerSrvrAddrs.h
        P2pNode.cpp       P2pNode.h
)
include_directories(.. ../misc ../inet ../repository)
