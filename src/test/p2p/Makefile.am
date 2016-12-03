AM_CPPFLAGS	= \
	-I$(top_srcdir)/main \
	-I$(top_srcdir)/main/misc \
	-I$(top_srcdir)/main/net \
	-I$(top_srcdir)/main/prod \
	-I$(top_srcdir)/main/p2p
LDADD		= $(top_builddir)/main/libhycast.la 
AM_LDFLAGS	= -lgtest -lsctp

check_PROGRAMS	= \
	Channel_test \
	ChunkChannel_test \
	Peer_test \
	PeerSet_test
#	Chunk2Peers_test

Channel_test_SOURCES		= Channel_test.cpp
ChunkChannel_test_SOURCES	= ChunkChannel_test.cpp
Peer_test_SOURCES		= Peer_test.cpp
#Chunk2Peers_test_SOURCES	= Chunk2Peers_test.cpp
PeerSet_test_SOURCES		= PeerSet_test.cpp

TESTS	= \
	Channel_test \
	ChunkChannel_test \
	Peer_test \
	PeerSet_test
#	Chunk2Peers_test

callgrind:	Peer_test
	rm -f callgrind.out.*
	$(LIBTOOL) --mode=execute valgrind --tool=callgrind ./Peer_test
	kcachegrind
	rm callgrind.out.*