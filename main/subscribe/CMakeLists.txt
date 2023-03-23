add_executable(subscribe subscribe.cpp)

target_link_libraries(subscribe hycast)

install(TARGETS subscribe DESTINATION bin)