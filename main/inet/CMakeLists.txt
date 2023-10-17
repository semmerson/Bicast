# Add the library
add_library(inet OBJECT
                        XprtAble.h
    InetAddr.cpp        InetAddr.h
    Xprt.cpp            Xprt.h
    SockAddr.cpp        SockAddr.h
    Socket.cpp          Socket.h
)