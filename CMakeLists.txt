cmake_minimum_required(VERSION 3.19)

# Set the project name and version
PROJECT(Hycast VERSION 1.0
        DESCRIPTION "Hybrid multicast/P2P protocol & data-distribution system")

## Include gRPC in the build
#option(USE_SYSTEM_GRPC "Use system installed gRPC" OFF)
#if(USE_SYSTEM_GRPC)
#  # Find system-installed gRPC
#  find_package(gRPC CONFIG REQUIRED)
#else()
#    include(FetchContent)
#    FetchContent_Declare(
#            gRPC
#            GIT_REPOSITORY https://github.com/grpc/grpc
#            GIT_TAG        v1.35.0)
#    set(FETCHCONTENT_QUIET OFF)
#    FetchContent_MakeAvailable(gRPC)
#endif()

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
    message(FATAL_ERROR "This package depends on the C++ std::reqex library, \
        which doesn't work in GNU C++ versions prior to 5")
endif()

# Specify the Eclipse IDE version
set(CMAKE_ECLIPSE_VERSION 4.17)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/.." CACHE PATH
        "Installation root directory" FORCE)
endif()

# Configure a configuration header file
configure_file(config-cmake.h config.h)
include_directories(${CMAKE_BINARY_DIR}) # necessary

include_directories(
        main main/misc main/inet main/mcast main/p2p main/repository main/node main/disposer)

# Find the OpenSSL library
find_library(OPEN_SSL_LIBRARY ssl REQUIRED)

# Add the main subdirectory
add_subdirectory(main)

# Enable testing if and only if Google Test can be found
find_library(GTEST_LIBRARY gtest)
if (GTEST_LIBRARY)
    find_path(GTEST_INCLUDE_DIR "gtest/gtest.h")
    if (NOT GTEST_INCLUDE_DIR)
        message(STATUS "Gtest header-file wasn't found. "
                "Unit-testing is disabled.")
    else()
        # This must be done *before* adding the testing subdirectory
        include(CTest)
        enable_testing()

        add_subdirectory(test)
        message(STATUS "Gtest was found. Unit-testing is enabled.")
    endif()
else()
    message(STATUS "Gtest library wasn't found. Unit-testing is disabled.")
endif()

# Install CHANGE_LOG and LICENSE
SET(CMAKE_INSTALL_DOCDIR share/doc/hycast)
install(FILES "${CMAKE_SOURCE_DIR}/CHANGE_LOG" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})

include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${Hycast_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${Hycast_VERSION_MINOR}")
include(CPack)
