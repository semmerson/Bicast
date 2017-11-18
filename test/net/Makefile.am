AM_CPPFLAGS	= -I$(top_srcdir)/main/misc -I$(top_srcdir)/main/net 
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= *.vg

check_PROGRAMS		= \
	socket_test \
	Interface_test \
	Ipv4Addr_test \
	Ipv6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	SctpSock_test \
	UdpSock_test \
	SerialInt_test

socket_test_SOURCES		= socket_test.cpp
Interface_test_SOURCES		= Interface_test.cpp
Ipv4Addr_test_SOURCES		= Ipv4Addr_test.cpp
Ipv6Addr_test_SOURCES		= Ipv6Addr_test.cpp
InetAddr_test_SOURCES		= InetAddr_test.cpp
InetSockAddr_test_SOURCES	= InetSockAddr_test.cpp
SctpSock_test_SOURCES		= SctpSock_test.cpp
UdpSock_test_SOURCES		= UdpSock_test.cpp
SerialInt_test_SOURCES		= SerialInt_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg: %
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes ./$< >$@ 2>&1