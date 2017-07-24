AM_CPPFLAGS	= -I$(top_srcdir)/main -I$(top_srcdir)/main/misc
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= *.vg

check_PROGRAMS	= thread_test \
		  unordered_map_test \
		  error_test \
		  Thread_test \
		  Task_test \
		  Promise_test \
		  Future_test \
		  Executor_test \
		  Completer_test

thread_test_SOURCES		= thread_test.cpp
unordered_map_test_SOURCES	= unordered_map_test.cpp
error_test_SOURCES		= error_test.cpp
Thread_test_SOURCES		= Thread_test.cpp
Task_test_SOURCES		= Task_test.cpp
Promise_test_SOURCES		= Promise_test.cpp
Future_test_SOURCES		= Future_test.cpp
Executor_test_SOURCES		= Executor_test.cpp
Completer_test_SOURCES		= Completer_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg: %
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes ./$< >$@ 2>&1