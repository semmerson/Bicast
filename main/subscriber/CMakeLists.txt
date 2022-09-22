add_executable(subscriber subscriber.cpp)

target_link_libraries(subscriber hycast)

install(TARGETS subscriber DESTINATION bin)