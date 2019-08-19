AM_CPPFLAGS	= -I$(top_srcdir)/main/misc \
		  -I$(top_srcdir)/main/net_io \
		  -I$(top_srcdir)/main/peer
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= *.vg

all:

check_PROGRAMS	= RemotePeer_test \
		  Peer_test \
		  PeerFactory_test \
		  PeerThreadPool_test \
		  PeerSet_test \
		  PeerSet2_test \
		  P2pMgr_test \
		  ThreadPool_test

RemotePeer_test_SOURCES		= RemotePeer_test.cpp
Peer_test_SOURCES		= Peer_test.cpp
PeerFactory_test_SOURCES	= PeerFactory_test.cpp
PeerThreadPool_test_SOURCES	= PeerThreadPool_test.cpp
PeerSet_test_SOURCES		= PeerSet_test.cpp
PeerSet2_test_SOURCES		= PeerSet2_test.cpp
P2pMgr_test_SOURCES		= P2pMgr_test.cpp
ThreadPool_test_SOURCES		= ThreadPool_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg: %
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes ./$< >$@ 2>&1
