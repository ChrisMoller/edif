AC_INIT([APL editor native function], [2.2.3], [moller@mollerware.com],[edif])
AC_PREFIX_DEFAULT( /usr/local )
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_HEADERS([edif_config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE([enable])

AC_PROG_CXX
AM_PROG_AR
#AM_PROG_LIBTOOL
LT_INIT

libdir=$libdir/apl

APL_SOURCES=`apl --show_src_dir`

AC_CHECK_FILE($APL_SOURCES, [foundit=yes], [foundit=no])
if test x$foundit = xno; then
AC_ARG_WITH([apl-source-dir],
  AS_HELP_STRING([--apl-source-dir=arg],
  [Location of the GNU APL source code.]))
  APL_SOURCES=$with_apl_source_dir
fi

AC_ARG_WITH([libnotify], AS_HELP_STRING([--with-libnotify],
[Build with libnotify]))

if test "x$with_libnotify" = "xyes"; then
  PKG_CHECK_MODULES([LIBNOTIFY],   [libnotify >= 0.7.9],
  [AC_DEFINE([HAVE_LIBNOTIFY], [1], [Use libnotify])])
  AC_SUBST(LIBNOTIFY_CFLAGS)
  AC_SUBST(LIBNOTIFY_LIBS)
fi

AC_ARG_WITH([apl], AS_HELP_STRING([--with-apl],
[Specify the APL source directory]))

if test "x$with_apl" != "x"; then
  APL_SOURCES=$with_apl
fi

AC_SUBST(APL_SOURCES)

AC_CONFIG_FILES([
  Makefile
  src/Makefile
])
AC_OUTPUT
