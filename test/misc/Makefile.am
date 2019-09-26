AM_CPPFLAGS	= -I$(top_srcdir)/main -I$(top_srcdir)/main/misc
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= *.vg
AM_CFLAGS	= -std=c99

check_PROGRAMS	= noexcept_test \
		  pImpl_test \
		  pimpl_test \
		  arg_passing_test \
		  thread_test \
		  unordered_map_test \
		  error_test \
		  Thread_test \
		  Task_test \
		  Future_test \
		  Executor_test \
		  Completer_test \
		  DelayQueue_test \
		  reuseaddr_test
#		  CancelNoexcept_test
#		  Promise_test

#CancelNoexcept_test_SOURCES	= CancelNoexcept_test.cpp
noexcept_test_SOURCES		= noexcept_test.cpp
pImpl_test_SOURCES		= pImpl_test.cpp
arg_passing_test_SOURCES	= arg_passing_test.cpp
pimpl_test_SOURCES		= pimpl_test.cpp
thread_test_SOURCES		= thread_test.cpp
unordered_map_test_SOURCES	= unordered_map_test.cpp
error_test_SOURCES		= error_test.cpp
Thread_test_SOURCES		= Thread_test.cpp
Task_test_SOURCES		= Task_test.cpp
#Promise_test_SOURCES		= Promise_test.cpp
Future_test_SOURCES		= Future_test.cpp
Executor_test_SOURCES		= Executor_test.cpp
Completer_test_SOURCES		= Completer_test.cpp
DelayQueue_test_SOURCES		= DelayQueue_test.cpp
reuseaddr_test_SOURCES		= reuseaddr_test.c
ReuseAddr_test_SOURCES		= ReuseAddr_test.cpp

TESTS	= $(check_PROGRAMS)

%.vg: %
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes ./$< >$@ 2>&1