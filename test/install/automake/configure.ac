AC_INIT([LibRalfAutoMakeProject], [1.0])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_LANG(C++)
LIBS="-lralf $LIBS"
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <ralf/LibRalf.h>], [LIBRALF_NS::libRalfVersionString()])],
    [],
    [AC_MSG_ERROR([libralf is not installed.])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
