cmake_minimum_required(VERSION 3.19)

# Set the project name and version
PROJECT(Hycast VERSION 1.0
        DESCRIPTION "Hybrid multicast/P2P protocol & data-distribution system")

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/.." CACHE PATH
        "Installation root directory" FORCE)
endif()

# Configure a configuration header file
configure_file(config-cmake.h config.h)
include_directories(${CMAKE_BINARY_DIR}) # necessary

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

# Install CHANGE_LOG and COPYING
SET(CMAKE_INSTALL_DOCDIR share/doc/hycast)
install(FILES "${CMAKE_SOURCE_DIR}/CHANGE_LOG" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})
install(FILES "${CMAKE_SOURCE_DIR}/COPYING" DESTINATION
            ${CMAKE_INSTALL_DOCDIR})

include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "${Hycast_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${Hycast_VERSION_MINOR}")
include(CPack)
