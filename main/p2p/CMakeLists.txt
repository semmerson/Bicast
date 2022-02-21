# Add the library
add_library(p2p OBJECT
        DataSeg.cpp
        Rpc.cpp           Rpc.h
        Peer.cpp          Peer.h
        Bookkeeper.cpp    Bookkeeper.h
        Tracker.cpp       Tracker.h
        P2pMgr.cpp        P2pMgr.h
)
#       NoticeArray.cpp   NoticeArray.h
#       PeerSet.cpp       PeerSet.h
#       Trigger.cpp       Trigger.h

include_directories(.. ../misc ../inet ../repository)
