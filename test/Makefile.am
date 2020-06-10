SUBDIRS			= misc inet protocol p2p repository node
AM_CPPFLAGS		= -I$(top_srcdir)/main/misc \
			  -I$(top_srcdir)/main/inet \
			  -I$(top_srcdir)/main/protocol \
			  -I$(top_srcdir)/main/p2p \
			  -I$(top_srcdir)/main/multicast
LDADD			= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES		= callgrind.out *.log *.vg *.supp

all:

#check_PROGRAMS		= hycast_test

#hycast_test_SOURCES	= hycast_test.cpp

TESTS			= $(check_PROGRAMS)