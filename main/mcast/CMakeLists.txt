# Add the library
add_library(protocol OBJECT
        Mcast.cpp Mcast.h
)
include_directories(. ../misc ../inet)