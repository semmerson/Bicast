add_executable(publisher publisher.cpp)

include_directories(../misc ../inet ../protocol ../p2p ../repository ../node)

target_link_libraries(publisher hycast)

install(TARGETS publisher DESTINATION bin)