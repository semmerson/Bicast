# Add the library
add_library(p2p OBJECT
        DataSeg.cpp
        Peer.cpp          Peer.h
#       NoticeArray.cpp   NoticeArray.h
        PeerSet.cpp       PeerSet.h
        Bookkeeper.cpp    Bookkeeper.h
        Tracker.cpp       Tracker.h
        P2pMgr.cpp        P2pMgr.h
)
include_directories(.. ../misc ../inet ../repository)
