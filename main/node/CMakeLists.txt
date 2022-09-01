# Add the library
add_library(node OBJECT
        NodeType.cpp    NodeType.h
        Node.cpp        Node.h
)
include_directories(
        ${CMAKE_SOURCE_DIR}/main
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
        ${CMAKE_SOURCE_DIR}/main/p2p
        ${CMAKE_SOURCE_DIR}/main/mcast
        ${CMAKE_SOURCE_DIR}/main/repository
)