AM_CPPFLAGS	= -I$(top_srcdir)/main \
                  -I$(top_srcdir)/main/misc \
                  -I$(top_srcdir)/main/net \
                  -I$(top_srcdir)/main/prod \
                  -I$(top_srcdir)/main/comms \
                  -I$(top_srcdir)/main/comms/mcast
LDADD		= $(top_builddir)/main/libhycast.la 
AM_LDFLAGS	= -lgtest -lsctp

check_PROGRAMS	= McastSender_test \
                  McastReceiver_test

McastSender_test_SOURCES	= McastSender_test.cpp
McastReceiver_test_SOURCES	= McastReceiver_test.cpp

TESTS	= $(check_PROGRAMS)