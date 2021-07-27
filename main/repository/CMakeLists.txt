# Add the library
add_library(repository OBJECT
        ProdFile.cpp   ProdFile.h
        Watcher.cpp    Watcher.h
        Repository.cpp Repository.h
)
include_directories(../misc ../inet ../p2p ..)