SUBDIRS			= misc net_io protocol p2p repository sender_receiver
AM_CPPFLAGS		= -I$(top_srcdir)/main/misc \
			  -I$(top_srcdir)/main/net_io \
			  -I$(top_srcdir)/main/protocol \
			  -I$(top_srcdir)/main/p2p \
			  -I$(top_srcdir)/main/multicast
LDADD			= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES		= callgrind.out *.log *.vg *.supp

all:

#check_PROGRAMS		= hycast_test

#hycast_test_SOURCES	= hycast_test.cpp

TESTS			= $(check_PROGRAMS)