AM_CPPFLAGS	= -I$(top_srcdir)/main/misc -I$(top_srcdir)/main/net_io
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= *.vg

all:

check_PROGRAMS		= \
	SockAddr_test \
	Socket_test \
	Wire_test

SockAddr_test_SOURCES		= SockAddr_test.cpp
Socket_test_SOURCES		= Socket_test.cpp
Wire_test_SOURCES		= Wire_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg: %
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes ./$< >$@ 2>&1