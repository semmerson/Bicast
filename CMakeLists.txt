cmake_minimum_required(VERSION 3.19)

# Set the project name and version
PROJECT(Bicast VERSION 0.0 DESCRIPTION "Combined multicast/P2P protocol & data-distribution system")

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
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
    message(FATAL_ERROR "This package depends on the C++ std::reqex library, \
        which doesn't work in GNU C++ versions prior to 5")
endif()

# Specify the Eclipse IDE version
set(CMAKE_ECLIPSE_VERSION 4.22)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Installation root directory" FORCE)
endif()

# Configure a configuration header file
configure_file(config.h.in config.h)
include_directories(${CMAKE_BINARY_DIR}) # necessary

include_directories(main main/misc main/inet main/mcast main/p2p main/repository main/node main/disposer)
include_directories(SYSTEM /usr/local/include)

# Add the main subdirectory
add_subdirectory(main)

# Enable testing if and only if Google Test can be found
find_library(GTEST_LIBRARY gtest)
if (GTEST_LIBRARY)
    find_path(GTEST_INCLUDE_DIR "gtest/gtest.h" HINTS /usr/include /usr/local/include)
    if (NOT GTEST_INCLUDE_DIR)
        message(STATUS "Gtest header-file wasn't found. Unit-testing is disabled.")
    else()
        # This must be done *before* adding the testing subdirectory
        include(CTest)
        enable_testing()

        add_subdirectory(test)
        message(STATUS "Gtest was found. Unit-testing is enabled.")
        message(STATUS "Gtest library=${GTEST_LIBRARY}.")
    endif()
else()
    message(STATUS "Gtest library wasn't found. Unit-testing is disabled.")
endif()

# Specify the documentation
find_package(Doxygen)
set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
set(DOXYGEN_ALIASES 
    [[exceptionsafety="@par Exception Safety:"]]
    [[threadsafety="@par Thread Safety:"]]
    [[asyncsignalsafe="@par Async-signal Safe:"]]
    [[cancellationpoint="@par Cancellation Point:"]])
set(DOXYGEN_VERBATIM_VARS DOXYGEN_ALIASES)
doxygen_add_docs(doxydocs mainpage.md main ALL)

# Install documentation
SET(CMAKE_INSTALL_DOCDIR share/doc)
install(DIRECTORY "${CMAKE_BINARY_DIR}/html" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})
install(FILES "${CMAKE_SOURCE_DIR}/CHANGE_LOG" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_CONTACT "Steven Emmerson <emmerson@ucar.edu>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/${PROJECT_NAME}-${PROJECT_VERSION}/")
set(CPACK_PACKAGE_VENDOR "University Corporation for Atmospheric Research")

SET(CPACK_DEBIAN_PACKAGE_DEPENDS "yaml-cpp-dev openssl11-dev")
SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_HOST_SYSTEM_PROCESSOR}")

SET(CPACK_RPM_PACKAGE_LICENSE "Apache 2.0")
SET(CPACK_RPM_PACKAGE_REQUIRES "yaml-cpp-devel openssl11-devel")
SET(CPACK_RPM_PACKAGE_PROVIDES "publish(1), subscribe(1)")

set(CPACK_GENERATOR DEB STGZ RPM) # RPM needs yum(1)-compatible UPC software repository
include(CPack)
