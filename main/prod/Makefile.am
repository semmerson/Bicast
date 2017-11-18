#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library

# Sources for libhycast
lib_la_SOURCES 	= HycastTypes.h \
		  ProdIndex.h \
		  ProdInfo.cpp ProdInfo.h \
		  ProdSize.h \
		  Chunk.cpp Chunk.h \
		  Product.cpp Product.h \
		  ProdRcvr.h \
		  ProdStore.cpp ProdStore.h \
		  Ingester.cpp Ingester.h \
		  FileIngester.cpp FileIngester.h

AM_CPPFLAGS = -I$(srcdir)/.. \
	      -I$(srcdir)/../misc \
              -I$(srcdir)/../net
