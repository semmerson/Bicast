#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library

# Sources for libhycast
lib_la_SOURCES 	= \
	logging.cpp logging.h \
	error.cpp error.h \
	DelayQueue.h \
	FixedDelayQueue.h \
	MapOfLists.cpp MapOfLists.h \
	Thread.h Thread.cpp \
	Future.cpp Future.h \
	Task.cpp Task.h \
	Executor.cpp Executor.h \
	Completer.cpp Completer.h
#	Promise.cpp Promise.h
#	SyncQueue.h

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
AM_CPPFLAGS = -I$(srcdir)/..