AC_INIT([gpsim_otgw], 1.0)
AC_PROG_CXX
LT_INIT([disable-static])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIRS([m4])

PKG_CHECK_MODULES([GLIB], [glib-2.0])

# Find the gpsim header files via either --with-gpsim, or pkg-config
AC_ARG_WITH([gpsim],
    AS_HELP_STRING([--with-gpsim], [directory containing the gpsim headers]))
AS_IF([test "x$with_gpsim" != "x"],
    [AC_SUBST([GPSIM_CFLAGS], ["-I$with_gpsim"])],
    [PKG_CHECK_MODULES([GPSIM], [gpsim >= 0.31.1])])

# printf modifier define for long long
AC_DEFINE([PRINTF_INT64_MODIFIER], ["ll"], [printf modifier define for long long])

AC_OUTPUT
