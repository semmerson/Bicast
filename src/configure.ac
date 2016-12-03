dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(hycast, 1.0)
AC_PROG_CXX
AC_SUBST([AM_CXXFLAGS], [-std=c++0x])

AC_CANONICAL_SYSTEM

AC_CONFIG_MACRO_DIR([m4])

dnl Initialize automake
AM_INIT_AUTOMAKE

dnl this allows us specify individual liking flags for each target
AM_PROG_CC_C_O 

dnl Initialize Libtool
LT_INIT

AC_CONFIG_FILES(Makefile
                main/Makefile
                main/misc/Makefile
                main/net/Makefile
                main/p2p/Makefile
                main/prod/Makefile
                test/Makefile
                test/net/Makefile
                test/prod/Makefile
                test/p2p/Makefile
                )
AC_OUTPUT