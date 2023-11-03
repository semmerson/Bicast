add_executable(publish publish.cpp RunPar.cpp)

target_link_libraries(publish bicast)

install(TARGETS publish DESTINATION bin)

# Add the configuration example
install(FILES publish_example.yaml DESTINATION etc)
