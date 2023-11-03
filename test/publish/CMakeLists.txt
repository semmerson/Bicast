add_executable(RunPar_test RunPar_test.cpp ${CMAKE_SOURCE_DIR}/main/publish/RunPar.cpp)
target_link_libraries(RunPar_test bicast gtest)
add_test(RunPar_test RunPar_test)
