#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
lib_LTLIBRARIES = libhycast.la

#######################################
# Build information for each library

# Sources for libhycast
libhycast_la_SOURCES 	= \
	PortNumber.h \
	Inet4Addr.cpp Inet4Addr.h \
	Inet6Addr.cpp Inet6Addr.h \
	InetAddrImpl.cpp InetAddrImpl.h \
	InetAddr.cpp InetAddr.h \
	InetSockAddrImpl.cpp InetSockAddrImpl.h \
	InetSockAddr.cpp InetSockAddr.h \
	PeerId.cpp PeerId.h \
        Socket.cpp Socket.h \
        PeerConnection.cpp PeerConnection.h \
        PendingPeerConnections.cpp PendingPeerConnections.h \
        PeerImpl.cpp PeerImpl.h

# Linker options libTestProgram
libhycast_la_LDFLAGS = 

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
libhycast_la_CPPFLAGS = -I$(top_srcdir)/include

check_PROGRAMS		= \
	Inet4Addr_test \
	Inet6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	PeerId_test \
	Socket_test \
	PendingPeerConnections_test \
	#Di_test

#Di_test_SOURCES	= Di_test.cpp
#Di_test_LDFLAGS	= -lgtest

Inet4Addr_test_SOURCES = Inet4Addr_test.cpp
Inet4Addr_test_LDFLAGS = libhycast.la -lgtest

Inet6Addr_test_SOURCES = Inet6Addr_test.cpp
Inet6Addr_test_LDFLAGS = libhycast.la -lgtest

InetAddr_test_SOURCES = InetAddr_test.cpp
InetAddr_test_LDFLAGS = libhycast.la -lgtest

InetSockAddr_test_SOURCES = InetSockAddr_test.cpp
InetSockAddr_test_LDFLAGS = libhycast.la -lgtest

PeerId_test_SOURCES = PeerId_test.cpp
PeerId_test_LDFLAGS = libhycast.la -lgtest

Socket_test_SOURCES = Socket_test.cpp
Socket_test_LDFLAGS = libhycast.la -lgtest

PendingPeerConnections_test_SOURCES = PendingPeerConnections_test.cpp
PendingPeerConnections_test_LDFLAGS = libhycast.la -lgtest

TESTS			= \
	Inet4Addr_test \
	Inet6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	PeerId_test \
	Socket_test \
        PendingPeerConnections_test \
        #Di_test