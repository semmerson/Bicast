add_executable(subscriber subscriber.cpp)

include_directories(.. ../misc ../inet ../p2p ../repository ../node)

target_link_libraries(subscriber hycast)

install(TARGETS subscriber DESTINATION bin)