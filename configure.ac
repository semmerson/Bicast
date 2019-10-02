dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(hycast, 1.0)
AC_PROG_CXX
AC_SUBST([AM_CXXFLAGS], [-std=c++11])
AC_DEFINE([_XOPEN_SOURCE], [700], [Level of X/OPEN compliance])

AC_CANONICAL_SYSTEM

AC_CONFIG_MACRO_DIR([m4])

dnl Initialize automake
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

dnl this allows us specify individual liking flags for each target
AM_PROG_CC_C_O 

dnl Initialize Libtool
LT_INIT

dnl Check if socket address structure has length field
AC_CHECK_MEMBERS(
    [struct ifreq.if_addr.sa_len], , , [[#include <net/if.h>]])

dnl Check for pthreads library
AC_SEARCH_LIBS([pthread_create], [pthread], ,
        AC_MSG_ERROR([Pthread library not found]))

dnl Check for SCTP library
AC_SEARCH_LIBS([sctp_recvmsg], [sctp], ,
        AC_MSG_ERROR([SCTP library not found]))

dnl Check for YAML library
AC_MSG_CHECKING([for library containing YAML::Node])
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([yaml-cpp/yaml.h], ,
  [AC_MSG_ERROR([yaml-cpp/yaml.h is not installed.])])
LIBS="-lyaml-cpp $LIBS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <yaml-cpp/yaml.h>], [YAML::Node node])],
  [AC_MSG_RESULT([-lyaml-cpp])], 
  [AC_MSG_ERROR([libyaml-cpp is not installed.])])
AC_LANG_POP()

dnl Check for IPv6 support
if ip addr | grep inet6 >/dev/null; then
  AC_DEFINE([SUPPORTS_IPV6], [1], ["Whether the system supports IPv6"])
fi

dnl Determine the name of the ethernet interface
name=`ip link | awk '/,MULTICAST,/ && /,UP,/ {print $2; exit}' | sed 's/://'`
AC_DEFINE_UNQUOTED([ETHNET_IFACE_NAME], ["$name"],
        ["Name of the ethernet interface"])

AC_CONFIG_FILES(Makefile
                main/Makefile
                main/misc/Makefile
                main/net_io/Makefile
                main/peer/Makefile
                test/Makefile
                test/misc/Makefile
                test/net_io/Makefile
                test/peer/Makefile
                test/storage/Makefile
                )
AC_OUTPUT
