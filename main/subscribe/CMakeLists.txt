add_executable(subscribe subscribe.cpp)

target_link_libraries(subscribe bicast)

install(TARGETS subscribe DESTINATION bin)

# Add the configuration example
install(FILES subscribe_example.yaml DESTINATION etc)
