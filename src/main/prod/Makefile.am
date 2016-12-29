#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library

# Sources for libhycast
lib_la_SOURCES 	= \
	HycastTypes.h \
	ProdIndex.cpp ProdIndex.h \
	ProdInfo.cpp ProdInfo.h \
	ChunkInfo.cpp ChunkInfo.h \
	Chunk.cpp Chunk.h \
	Product.cpp Product.h \
	ProdFactory.cpp ProdFactory.h

lib_la_CPPFLAGS = -I$(srcdir)/../net