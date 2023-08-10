add_executable(publish publish.cpp)

target_link_libraries(publish hycast)

install(TARGETS publish DESTINATION bin)

# Add the configuration example
install(FILES publish_example.yaml DESTINATION etc)