# Add the library
add_library(mcast OBJECT
        mcast.cpp       mcast.h
)

include_directories(.. ../misc ../inet ../node)
