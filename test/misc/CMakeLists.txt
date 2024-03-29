add_library(dummy OBJECT Diamond.cpp)

#add_executable(AutoReference AutoReference.cpp)
#add_test(AutoReference AutoReference)

add_executable(PimplIface PimplIface.cpp)
add_test(PimplIface PimplIface)

add_executable(noexcept_test noexcept_test.cpp)
target_link_libraries(noexcept_test gtest pthread)
add_test(noexcept_test noexcept_test)

add_executable(Pimpl_test Pimpl_test.cpp)
target_link_libraries(Pimpl_test gtest pthread)
add_test(Pimpl_test Pimpl_test)

add_executable(pimpl_test pimpl_test.cpp)
add_test(pimpl_test pimpl_test)

add_executable(arg_passing_test arg_passing_test.cpp)
target_link_libraries(arg_passing_test gtest pthread)
add_test(arg_passing_test arg_passing_test)

add_executable(thread_test thread_test.cpp)
target_link_libraries(thread_test gtest pthread)
add_test(thread_test thread_test)

add_executable(unordered_map_test unordered_map_test.cpp)
target_link_libraries(unordered_map_test gtest pthread)
add_test(unordered_map_test unordered_map_test)

add_executable(error_test error_test.cpp)
target_link_libraries(error_test bicast gtest pthread)
add_test(error_test error_test)

#add_executable(MapQ_test MapQ_test.cpp)
#target_link_libraries(MapQ_test bicast gtest pthread)
#add_test(MapQ_test MapQ_test)

#add_executable(DelayQueue_test DelayQueue_test.cpp)
#target_link_libraries(DelayQueue_test bicast gtest pthread)
#add_test(DelayQueue_test DelayQueue_test)

add_executable(HashSetQueue_test HashSetQueue_test.cpp)
target_link_libraries(HashSetQueue_test bicast ${GTEST_LIBRARY} pthread)
add_test(HashSetQueue_test HashSetQueue_test)

add_executable(HashMapQueue_test HashMapQueue_test.cpp)
target_link_libraries(HashMapQueue_test bicast gtest pthread)
add_test(HashMapQueue_test HashMapQueue_test)

add_executable(reuseaddr_test reuseaddr_test.c)
target_link_libraries(reuseaddr_test bicast pthread)
add_test(reuseaddr_test reuseaddr_test)
