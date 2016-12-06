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
	ProdInfoImpl.cpp ProdInfoImpl.h \
	ProdInfo.cpp ProdInfo.h \
	ChunkInfo.cpp ChunkInfo.h \
	ChunkImpl.cpp ChunkImpl.h \
	Chunk.cpp Chunk.h \
	ProductImpl.cpp ProductImpl.h \
	Product.cpp Product.h \
	ProdFactoryImpl.cpp ProdFactoryImpl.h \
	ProdFactory.cpp ProdFactory.h

lib_la_CPPFLAGS = -I$(srcdir)/../net