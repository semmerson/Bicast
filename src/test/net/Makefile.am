AM_CPPFLAGS	= -I$(top_srcdir)/main/net
AM_LDFLAGS	= $(top_builddir)/main/libhycast.la -lgtest -lsctp

check_PROGRAMS		= \
	Ipv4Addr_test \
	Ipv6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	SctpSock_test

Ipv4Addr_test_SOURCES		= Ipv4Addr_test.cpp
Ipv6Addr_test_SOURCES		= Ipv6Addr_test.cpp
InetAddr_test_SOURCES		= InetAddr_test.cpp
InetSockAddr_test_SOURCES	= InetSockAddr_test.cpp
SctpSock_test_SOURCES		= SctpSock_test.cpp

TESTS	= \
	Ipv4Addr_test \
	Ipv6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	SctpSock_test
