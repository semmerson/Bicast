# Add the library
add_library(inet OBJECT
    InetAddr.cpp        InetAddr.h
    Xprt.cpp            Xprt.h
    SockAddr.cpp        SockAddr.h
    Socket.cpp          Socket.h
    PortPool.cpp        PortPool.h
)