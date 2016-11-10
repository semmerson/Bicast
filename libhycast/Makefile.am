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
	InetAddrImpl.cpp InetAddrImpl.h \
	IpAddr.cpp IpAddr.h \
	Inet4Addr.cpp Inet4Addr.h \
	Inet6Addr.cpp Inet6Addr.h \
	InetNameAddr.cpp InetNameAddr.h \
	InetAddr.cpp InetAddr.h \
	InetSockAddrImpl.cpp InetSockAddrImpl.h \
	InetSockAddr.cpp InetSockAddr.h \
	ClientSocketImpl.cpp ClientSocketImpl.h \
	ClientSocket.cpp ClientSocket.h \
	ServerSocketImpl.cpp ServerSocketImpl.h \
	ServerSocket.cpp ServerSocket.h \
	SocketImpl.cpp SocketImpl.h \
	Socket.cpp Socket.h \
	Serializable.h \
	ProdIndex.cpp ProdIndex.h \
	ProdInfoImpl.cpp ProdInfoImpl.h \
	ProdInfo.cpp ProdInfo.h \
	ChunkInfo.cpp ChunkInfo.h \
	ChunkImpl.cpp ChunkImpl.h \
	Chunk.cpp Chunk.h \
	VersionMsg.cpp VersionMsg.h \
	Channel.h \
	ChannelImpl.cpp ChannelImpl.h \
	RegChannel.cpp RegChannel.h \
	RegChannelImpl.cpp RegChannelImpl.h \
	ChunkChannelImpl.cpp ChunkChannelImpl.h \
	ChunkChannel.cpp ChunkChannel.h \
	PeerImpl.cpp PeerImpl.h \
	Peer.cpp Peer.h \
	PeerMgr.h \
	MapOfLists.cpp MapOfLists.h \
	PeerSetImpl.cpp PeerSetImpl.h \
	PeerSet.cpp PeerSet.h

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
	Socket_test \
	ProdIndex_test \
	ProdInfo_test \
	ChunkInfo_test \
	Channel_test \
	ChunkChannel_test \
	Peer_test \
	Chunk2Peers_test \
	PeerSet_test \
	#Di_test

#Di_test_SOURCES	= Di_test.cpp
#Di_test_LDFLAGS	= -lgtest

Inet4Addr_test_SOURCES = Inet4Addr_test.cpp
Inet4Addr_test_LDFLAGS = libhycast.la -lgtest -lsctp

Inet6Addr_test_SOURCES = Inet6Addr_test.cpp
Inet6Addr_test_LDFLAGS = libhycast.la -lgtest -lsctp

InetAddr_test_SOURCES = InetAddr_test.cpp
InetAddr_test_LDFLAGS = libhycast.la -lgtest -lsctp

InetSockAddr_test_SOURCES = InetSockAddr_test.cpp
InetSockAddr_test_LDFLAGS = libhycast.la -lgtest -lsctp

Socket_test_SOURCES = Socket_test.cpp
Socket_test_LDFLAGS = libhycast.la -lgtest -lsctp

ProdIndex_test_SOURCES = ProdIndex_test.cpp
ProdIndex_test_LDFLAGS = libhycast.la -lgtest -lsctp

ProdInfo_test_SOURCES = ProdInfo_test.cpp
ProdInfo_test_LDFLAGS = libhycast.la -lgtest -lsctp

ChunkInfo_test_SOURCES = ChunkInfo_test.cpp
ChunkInfo_test_LDFLAGS = libhycast.la -lgtest -lsctp

Channel_test_SOURCES = Channel_test.cpp
Channel_test_LDFLAGS = libhycast.la -lgtest -lsctp

ChunkChannel_test_SOURCES = ChunkChannel_test.cpp
ChunkChannel_test_LDFLAGS = libhycast.la -lgtest -lsctp

Peer_test_SOURCES = Peer_test.cpp
Peer_test_LDFLAGS = libhycast.la -lgtest -lsctp

Chunk2Peers_test_SOURCES = Chunk2Peers_test.cpp
Chunk2Peers_test_LDFLAGS = libhycast.la -lgtest -lsctp

PeerSet_test_SOURCES = PeerSet_test.cpp
PeerSet_test_LDFLAGS = libhycast.la -lgtest -lsctp

TESTS			= \
	Inet4Addr_test \
	Inet6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	Socket_test \
	ProdIndex_test \
	ProdInfo_test \
	ChunkInfo_test \
	Channel_test \
	ChunkChannel_test \
	Peer_test \
	Chunk2Peers_test \
	PeerSet_test \
        #Di_test