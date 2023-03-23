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

add_subdirectory(misc)
add_subdirectory(inet)
add_subdirectory(p2p)
add_subdirectory(mcast)
add_subdirectory(repository)
add_subdirectory(disposer)
add_subdirectory(node)
add_subdirectory(lib)
add_subdirectory(publish)
add_subdirectory(subscribe)

# Check for Doxygen(1)
find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "doxygen(1) wasn't found. Documentation can't be created.")
endif()