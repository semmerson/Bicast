#add_executable(AutoReference AutoReference.cpp)
#add_test(AutoReference AutoReference)

add_executable(noexcept_test noexcept_test.cpp)
target_link_libraries(noexcept_test gtest pthread)
add_test(noexcept_test noexcept_test)

add_executable(pImpl_test pImpl_test.cpp)
target_link_libraries(pImpl_test gtest)
add_test(pImpl_test pImpl_test)

add_executable(pimpl_test pimpl_test.cpp)
add_test(pimpl_test pimpl_test)

add_executable(arg_passing_test arg_passing_test.cpp)
target_link_libraries(arg_passing_test gtest)
add_test(arg_passing_test arg_passing_test)

add_executable(thread_test thread_test.cpp)
target_link_libraries(thread_test gtest pthread)
add_test(thread_test thread_test)

add_executable(unordered_map_test unordered_map_test.cpp)
target_link_libraries(unordered_map_test gtest)
add_test(unordered_map_test unordered_map_test)

add_executable(error_test error_test.cpp)
include_directories(${CMAKE_SOURCE_DIR}/main/misc)
target_link_libraries(error_test hycast gtest)
add_test(error_test error_test)

add_executable(DelayQueue_test DelayQueue_test.cpp)
target_link_libraries(DelayQueue_test hycast gtest)
add_test(DelayQueue_test DelayQueue_test)

add_executable(reuseaddr_test reuseaddr_test.c)
target_link_libraries(reuseaddr_test hycast pthread)
add_test(reuseaddr_test reuseaddr_test)