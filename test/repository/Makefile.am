AM_CPPFLAGS	= -I$(top_srcdir)/main/misc \
		  -I$(top_srcdir)/main/inet \
		  -I$(top_srcdir)/main/protocol \
		  -I$(top_srcdir)/main/repository
LDADD		= $(top_builddir)/main/libhycast.la -lgtest
CLEANFILES	= callgrind.out *.log *.vg *.supp

all:

SUFFIXES: .i .cpp
.cpp.i:
	$(CXXCOMPILE) $(AM_CXXFLAGS) -E -o $@ $<
	      
SUFFIXES:	.vg .sup .supp

check_PROGRAMS	= ProdFile_test \
                  Watcher_test \
                  Repository_test

ProdFile_test_SOURCES	= ProdFile_test.cpp
Watcher_test_SOURCES	= Watcher_test.cpp
Repository_test_SOURCES	= Repository_test.cpp

TESTS	= $(check_PROGRAMS)

SUFFIXES:	.vg .sup .supp

# Precursor to Valgrind suppression file
%.supp:	%
	test -e $<.sup && supOpt=--suppressions=$<.sup; \
	$(LIBTOOL) --mode=execute valgrind --leak-check=full \
            --show-reachable=yes $${supOpt} --gen-suppressions=all ./$< >&$@

# Valgrind output
%.vg:	%
	test -e $<.sup && supOpt=--suppressions=$<.sup; \
	$(LIBTOOL) --mode=execute valgrind --leak-check=full \
            --show-reachable=yes $${supOpt} ./$< >&$@