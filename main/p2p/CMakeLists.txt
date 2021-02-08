# Add the library
add_library(p2p OBJECT
        Peer.cpp 		Peer.h
        PeerFactory.cpp	        PeerFactory.h
        PeerSet.cpp		PeerSet.h
        ServerPool.cpp	        ServerPool.h
        P2pMgr.cpp		P2pMgr.h
        Bookkeeper.cpp	        Bookkeeper.h
        ChunkIdQueue.cpp	ChunkIdQueue.h
)
include_directories(../misc ../inet ../protocol ../node ../repository)