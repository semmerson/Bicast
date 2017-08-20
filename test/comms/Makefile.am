SUBDIRS	= mcast p2p

AM_CPPFLAGS	= -I$(top_srcdir)/main \
                  -I$(top_srcdir)/main/misc \
                  -I$(top_srcdir)/main/net \
                  -I$(top_srcdir)/main/prod \
                  -I$(top_srcdir)/main/comms \
                  -I$(top_srcdir)/main/comms/mcast \
                  -I$(top_srcdir)/main/comms/p2p
LDADD		= $(top_builddir)/main/libhycast.la -lgtest

check_PROGRAMS	= ShipRecv_test

ShipRecv_test_SOURCES		= ShipRecv_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg: %
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes ./$< >$@ 2>&1