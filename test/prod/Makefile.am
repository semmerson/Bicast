AM_CPPFLAGS	= -I$(top_srcdir)/main/net \
		  -I$(top_srcdir)/main/prod \
		  -I$(top_srcdir)/main/misc
LDADD		= $(top_builddir)/main/libhycast.la -lgtest

check_PROGRAMS	= ProdIndex_test \
		  ProdInfo_test \
		  ChunkInfo_test \
		  Product_test \
		  ProdFactory_test \
		  UnknownProd_test \
		  ProdStore_test \
		  FileIngester_test

ProdIndex_test_SOURCES		= ProdIndex_test.cpp
ProdInfo_test_SOURCES		= ProdInfo_test.cpp
ChunkInfo_test_SOURCES		= ChunkInfo_test.cpp
Product_test_SOURCES		= Product_test.cpp
ProdFactory_test_SOURCES	= ProdFactory_test.cpp
UnknownProd_test_SOURCES	= UnknownProd_test.cpp
ProdStore_test_SOURCES		= ProdStore_test.cpp
FileIngester_test_SOURCES	= FileIngester_test.cpp

TESTS		= $(check_PROGRAMS)