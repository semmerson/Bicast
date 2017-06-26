AM_CPPFLAGS	= -I$(top_srcdir)/main -I$(top_srcdir)/main/misc
LDADD		= $(top_builddir)/main/libhycast.la 
AM_LDFLAGS	= -lgtest -lsctp
CLEANFILES	= *.vg

check_PROGRAMS	= unordered_map_test \
		  error_test \
		  Thread_test \
		  Promise_test \
		  TaskFuture_test \
		  Executor_test \
		  Completer_test

unordered_map_test_SOURCES	= unordered_map_test.cpp
error_test_SOURCES		= error_test.cpp
Thread_test_SOURCES		= Thread_test.cpp
Promise_test_SOURCES		= Promise_test.cpp
TaskFuture_test_SOURCES		= TaskFuture_test.cpp
Executor_test_SOURCES		= Executor_test.cpp
Completer_test_SOURCES		= Completer_test.cpp

TESTS	= unordered_map_test \
	  error_test \
	  Thread_test \
	  Promise_test \
	  TaskFuture_test \
	  Executor_test \
	  Completer_test

Thread_test.vg:	Thread_test
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes \
	./Thread_test >$@ 2>&1

Promise_test.vg:	Promise_test
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes \
	./Promise_test >$@ 2>&1

TaskFuture_test.vg:	TaskFuture_test
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes \
	./TaskFuture_test >$@ 2>&1

Executor_test.vg:	Executor_test
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes \
	./Executor_test >$@ 2>&1

Completer_test.vg:	Completer_test
	$(TESTS_ENVIRONMENT) $(LIBTOOL) --mode=execute valgrind \
	--leak-check=full --show-reachable=yes \
	./Completer_test >$@ 2>&1
