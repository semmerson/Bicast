# Add the library
add_library(p2p OBJECT
        DataSeg.cpp
        Rpc.cpp           Rpc.h
        P2pSrvrInfo.cpp   P2pSrvrInfo.h
        Tracker.cpp       Tracker.h
        Notice.cpp        Notice.h
        Peer.cpp          Peer.h
        PeerConn.cpp      PeerConn.h
        Bookkeeper.cpp    Bookkeeper.h
        P2pMgr.cpp        P2pMgr.h
)
#       NoticeArray.cpp   NoticeArray.h
#       PeerSet.cpp       PeerSet.h
#       Trigger.cpp       Trigger.h