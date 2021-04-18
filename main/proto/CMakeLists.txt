# Add the library
add_library(proto OBJECT
                 HycastProto.h
        Peer.cpp Peer.h
                 Node.h
)
include_directories(../misc ../inet)
