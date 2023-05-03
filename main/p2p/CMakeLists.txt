# Add the library
add_library(p2p OBJECT
        DataSeg.cpp
        Tracker.cpp       Tracker.h
        Rpc.cpp           Rpc.h
        Peer.cpp          Peer.h
        PeerConn.cpp      PeerConn.h
        Bookkeeper.cpp    Bookkeeper.h
        P2pMgr.cpp        P2pMgr.h
)
#       NoticeArray.cpp   NoticeArray.h
#       PeerSet.cpp       PeerSet.h
#       Trigger.cpp       Trigger.h