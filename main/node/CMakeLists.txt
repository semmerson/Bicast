# Add the library
add_library(node OBJECT
                        NodeType.h
        Node.cpp	Node.h
)
include_directories(. ../misc ../inet ../protocol ../p2p ../repository)