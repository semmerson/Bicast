add_executable(publish publish.cpp)

target_link_libraries(publish hycast)

install(TARGETS publish DESTINATION bin)