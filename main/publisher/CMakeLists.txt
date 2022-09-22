add_executable(publisher publisher.cpp)

target_link_libraries(publisher hycast)

install(TARGETS publisher DESTINATION bin)