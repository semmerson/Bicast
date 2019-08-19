#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library

# Library Sources
lib_la_SOURCES 	= \
	InAddr.cpp	InAddr.h \
	SockAddr.cpp    SockAddr.h \
	Socket.cpp      Socket.h \
	Wire.cpp	Wire.h
#	ServiceId.cpp ServiceId.h
#	IoVec.cpp IoVec.h \
#	Serializable.h \
#	Wire.h

# Compiler options. Here we are adding the include directory to be searched for
# headers included in the source code.
AM_CPPFLAGS = -I$(srcdir)/.. \
	      -I$(srcdir)/../misc
