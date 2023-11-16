add_executable(RunPar_test RunPar_test.cpp)
target_link_libraries(RunPar_test bicast gtest)
add_test(RunPar_test RunPar_test)
