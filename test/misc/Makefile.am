AM_CPPFLAGS	= -I$(top_srcdir)/main -I$(top_srcdir)/main/misc
LDADD		= $(top_builddir)/main/libhycast.la 
AM_LDFLAGS	= -lgtest -lsctp

check_PROGRAMS	= Executor_test Completer_test error_test

Executor_test_SOURCES		= Executor_test.cpp
Completer_test_SOURCES		= Completer_test.cpp
error_test_SOURCES		= error_test.cpp

TESTS	= Executor_test Completer_test error_test