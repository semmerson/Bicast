AM_CPPFLAGS	= -I$(top_srcdir)/main/misc \
		  -I$(top_srcdir)/main/inet \
		  -I$(top_srcdir)/main/protocol \
		  -I$(top_srcdir)/main/p2p
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= callgrind.out *.log *.vg *.supp

all:

check_PROGRAMS	= ChunkIdQueue_test \
		  Peer_test \
		  PeerFactory_test \
		  PeerSet_test \
		  P2pMgr_test

Peer_test_SOURCES		= Peer_test.cpp
PeerFactory_test_SOURCES	= PeerFactory_test.cpp
#PeerThreadPool_test_SOURCES	= PeerThreadPool_test.cpp
ChunkIdQueue_test_SOURCES	= ChunkIdQueue_test.cpp
PeerSet_test_SOURCES		= PeerSet_test.cpp
#PeerSet2_test_SOURCES		= PeerSet2_test.cpp
P2pMgr_test_SOURCES		= P2pMgr_test.cpp
#P2pMgr_test2_SOURCES		= P2pMgr_test2.cpp
#ThreadPool_test_SOURCES	= ThreadPool_test.cpp

TESTS	= $(check_PROGRAMS)

SUFFIXES:	.vg .sup .supp

# Precursor to Valgrind suppression file
%.supp:	%
	test -e $<.sup && supOpt=--suppressions=$<.sup; \
	$(LIBTOOL) --mode=execute valgrind --leak-check=full \
            --show-reachable=yes $${supOpt} --gen-suppressions=all ./$< >&$@

# Valgrind output
%.vg:	%
	test -e $<.sup && supOpt=--suppressions=$<.sup; \
	$(LIBTOOL) --mode=execute valgrind --leak-check=full \
            --show-reachable=yes $${supOpt} ./$< >&$@
