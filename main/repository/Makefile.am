#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
#noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library

# Library Sources
#lib_la_SOURCES 	= #MemProd.cpp MemProd.h
EXTRA_DIST		= Repository.h

# Compiler options. Here we are adding the include directory to be searched for
# headers included in the source code.
AM_CPPFLAGS = -I$(srcdir)/.. \
	      -I$(srcdir)/../misc \
	      -I$(srcdir)/../net_io \
	      -I$(srcdir)/../protocol
	      
SUFFIXES: .i .cpp
.cpp.i:
	$(COMPILE) $(AM_CXXFLAGS) -E -o $@ $<
