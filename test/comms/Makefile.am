SUBDIRS	= mcast p2p

AM_CPPFLAGS	= -I$(top_srcdir)/main \
                  -I$(top_srcdir)/main/misc \
                  -I$(top_srcdir)/main/net \
                  -I$(top_srcdir)/main/prod \
                  -I$(top_srcdir)/main/comms \
                  -I$(top_srcdir)/main/comms/mcast \
                  -I$(top_srcdir)/main/comms/p2p
LDADD		= $(top_builddir)/main/libhycast.la -lgtest

check_PROGRAMS	= Receiving_test \
				  Shipping_test

Receiving_test_SOURCES		= Receiving_test.cpp
Shipping_test_SOURCES		= Shipping_test.cpp

TESTS	= Receiving_test \
		  Shipping_test