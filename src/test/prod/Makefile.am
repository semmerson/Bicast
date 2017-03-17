AM_CPPFLAGS	= -I$(top_srcdir)/main/net -I$(top_srcdir)/main/prod
AM_LDFLAGS	= $(top_builddir)/main/libhycast.la -lgtest -lsctp

check_PROGRAMS	= ProdIndex_test \
		  ProdInfo_test \
		  ChunkInfo_test \
		  Product_test \
		  ProdFactory_test \
		  UnknownProd_test \
		  ProdStore_test

ProdIndex_test_SOURCES		= ProdIndex_test.cpp
ProdInfo_test_SOURCES		= ProdInfo_test.cpp
ChunkInfo_test_SOURCES		= ChunkInfo_test.cpp
Product_test_SOURCES		= Product_test.cpp
ProdFactory_test_SOURCES	= ProdFactory_test.cpp
UnknownProd_test_SOURCES	= UnknownProd_test.cpp
ProdStore_test_SOURCES		= ProdStore_test.cpp

TESTS		= ProdIndex_test \
		  ProdInfo_test \
		  ChunkInfo_test \
		  Product_test \
		  ProdFactory_test \
		  UnknownProd_test \
		  ProdStore_test