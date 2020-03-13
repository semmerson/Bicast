AM_CPPFLAGS	= -I$(top_srcdir)/main/misc \
		  -I$(top_srcdir)/main/net_io \
		  -I$(top_srcdir)/main/protocol \
		  -I$(top_srcdir)/main/p2p \
		  -I$(top_srcdir)/main/multicast
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= callgrind.out *.log *.vg *.supp

all:

check_PROGRAMS	= PeerProto_test McastProto_test

PeerProto_test_SOURCES		= PeerProto_test.cpp
McastProto_test_SOURCES		= McastProto_test.cpp

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
