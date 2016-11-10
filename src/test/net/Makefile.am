AM_CPPFLAGS	= -I$(top_srcdir)/main/net
AM_LDFLAGS	= $(top_builddir)/main/libhycast.la -lgtest -lsctp

check_PROGRAMS		= \
	Inet4Addr_test \
	Inet6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	Socket_test

Inet4Addr_test_SOURCES		= Inet4Addr_test.cpp
Inet6Addr_test_SOURCES		= Inet6Addr_test.cpp
InetAddr_test_SOURCES		= InetAddr_test.cpp
InetSockAddr_test_SOURCES	= InetSockAddr_test.cpp
Socket_test_SOURCES		= Socket_test.cpp

TESTS	= \
	Inet4Addr_test \
	Inet6Addr_test \
	InetAddr_test \
	InetSockAddr_test \
	Socket_test
