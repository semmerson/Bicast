AM_CPPFLAGS	= -I$(top_srcdir)/main/misc \
		  -I$(top_srcdir)/main/net_io \
		  -I$(top_srcdir)/main/protocol \
		  -I$(top_srcdir)/main/p2p \
		  -I$(top_srcdir)/main/repository \
		  -I$(top_srcdir)/main/sender_receiver
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= callgrind.out *.log *.vg *.supp

all:

SUFFIXES: .i .cpp
.cpp.i:
	$(CXXCOMPILE) $(AM_CXXFLAGS) -E -o $@ $<

check_PROGRAMS		= SndrRcvr_test

SndrRcvr_test_SOURCES	= SndrRcvr_test.cpp

TESTS			= $(check_PROGRAMS)

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