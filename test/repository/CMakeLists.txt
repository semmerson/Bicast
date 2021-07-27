include_directories(
        ${CMAKE_SOURCE_DIR}/main
        ${CMAKE_SOURCE_DIR}/main/repository
        ${CMAKE_SOURCE_DIR}/main/p2p
        ${CMAKE_SOURCE_DIR}/main/misc
        ${CMAKE_SOURCE_DIR}/main/inet
)

add_executable(ProdFile_test ProdFile_test.cpp)
target_link_libraries(ProdFile_test hycast gtest)
add_test(ProdFile_test ProdFile_test)

add_executable(Watcher_test Watcher_test.cpp)
target_link_libraries(Watcher_test hycast gtest)
add_test(Watcher_test Watcher_test)

add_executable(Repository_test Repository_test.cpp)
target_link_libraries(Repository_test hycast gtest)
add_test(Repository_test Repository_test)