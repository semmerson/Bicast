AM_CPPFLAGS	= -I$(top_srcdir)/main \
                  -I$(top_srcdir)/main/misc \
                  -I$(top_srcdir)/main/net \
                  -I$(top_srcdir)/main/prod \
                  -I$(top_srcdir)/main/comms \
                  -I$(top_srcdir)/main/comms/mcast \
                  -I$(top_srcdir)/main/comms/p2p
LDADD		= $(top_builddir)/main/libhycast.la 
AM_LDFLAGS	= -lgtest -lsctp
CLEANFILES	= *.vg

check_PROGRAMS	= Channel_test \
                  ChunkChannel_test \
                  Peer_test \
                  PeerSet_test \
                  P2pMgr_test
#                  CancelDestroy_test
#	          Chunk2Peers_test

Channel_test_SOURCES		= Channel_test.cpp
ChunkChannel_test_SOURCES	= ChunkChannel_test.cpp
Peer_test_SOURCES		= Peer_test.cpp
#Chunk2Peers_test_SOURCES	= Chunk2Peers_test.cpp
PeerSet_test_SOURCES		= PeerSet_test.cpp
P2pMgr_test_SOURCES		= P2pMgr_test.cpp
#CancelDestroy_test_SOURCES      = CancelDestroy_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg:	%
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
                --leak-check=full --show-reachable=yes ./$< >$@ 2>&1

callgrind:	Peer_test
	rm -f callgrind.out.*
	$(LIBTOOL) --mode=execute valgrind --tool=callgrind ./Peer_test
	kcachegrind
	rm callgrind.out.*
