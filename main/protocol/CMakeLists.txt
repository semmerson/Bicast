# Add the library
add_library(protocol OBJECT
        hycast.cpp     hycast.h
        PeerProto.cpp  PeerProto.h
        McastProto.cpp McastProto.h
)
include_directories(. ../misc ../inet ../p2p ../node ../repository)