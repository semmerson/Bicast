# Build a shared library by default
option(BUILD_SHARED_LIBS "Build a shared library" ON)
if(BUILD_SHARED_LIBS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(CMAKE_BUILD_RPATH "@executable_path/../../lib")
        set(CMAKE_INSTALL_RPATH "@executable_path/../../lib")
    else()
        message(WARNING "Relocatable binary distribution is not supported for "
                "O/S \"${CMAKE_SYSTEM_NAME}\"")
    endif()
endif()

add_library(main OBJECT
        HycastProto.cpp HycastProto.h
)
include_directories(misc)
include_directories(inet)

add_subdirectory(misc)
add_subdirectory(inet)
add_subdirectory(p2p)
add_subdirectory(lib)
add_subdirectory(repository)

# Check for Doxygen(1)
find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen wasn't found. Documentation won't be created "
            "if it doesn't already exist")
endif()