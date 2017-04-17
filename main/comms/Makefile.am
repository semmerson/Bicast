#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library
SUBDIRS			= mcast p2p
lib_la_LIBADD		= mcast/lib.la p2p/lib.la

# Sources for libhycast
lib_la_SOURCES 	= Receiving.cpp Receiving.h \
		  MsgRcvrImpl.h MsgRcvr.h \
		  Notifier.h \
		  Shipping.cpp Shipping.h \
		  Processing.h

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
AM_CPPFLAGS = -I$(srcdir)/.. \
              -I$(srcdir)/mcast \
              -I$(srcdir)/p2p \
              -I$(srcdir)/../prod \
              -I$(srcdir)/../net \
	      -I$(srcdir)/../misc
