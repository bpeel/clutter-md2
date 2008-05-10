# clutter-md2 package version number, (as distinct from shared library
# version) An odd micro number indicates in-progress development,
# (eg. from Git) An even micro number indicates a released version.
m4_define([clutter_md2_major_version], [0])
m4_define([clutter_md2_minor_version], [1])
m4_define([clutter_md2_micro_version], [1])

m4_define([clutter_md2_version],
          [clutter_md2_major_version.clutter_md2_minor_version.clutter_md2_micro_version])

m4_define([clutter_md2_api_version],
          [clutter_md2_major_version.clutter_md2_minor_version])

# increase the interface age for each release; if the API changes, set to 0
m4_define([clutter_md2_interface_age], [0])
m4_define([clutter_md2_binary_age],
          [m4_eval(100 * clutter_md2_minor_version
	           + clutter_md2_micro_version)])

AC_PREREQ(2.53)

AC_INIT([clutter-md2],
        [clutter_md2_version])
AC_CONFIG_SRCDIR([clutter-md2/clutter-md2.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.9])

CLUTTER_MD2_MAJOR_VERSION=clutter_md2_major_version
CLUTTER_MD2_MINOR_VERSION=clutter_md2_minor_version
CLUTTER_MD2_MICRO_VERSION=clutter_md2_micro_version
CLUTTER_MD2_VERSION=clutter_md2_version
CLUTTER_MD2_API_VERSION=clutter_md2_api_version
CLUTTER_MD2_MAJORMINOR=clutter_md2_api_version
AC_SUBST(CLUTTER_MD2_MAJOR_VERSION)
AC_SUBST(CLUTTER_MD2_MINOR_VERSION)
AC_SUBST(CLUTTER_MD2_MICRO_VERSION)
AC_SUBST(CLUTTER_MD2_VERSION)
AC_SUBST(CLUTTER_MD2_API_VERSION)
AC_SUBST(CLUTTER_MD2_MAJORMINOR)

m4_define([lt_current],
          [m4_eval(100 * clutter_md2_minor_version
                   + clutter_md2_micro_version - clutter_md2_interface_age)])
m4_define([lt_revision], [clutter_md2_interface_age])
m4_define([lt_age], [m4_eval(clutter_md2_binary_age
                             - clutter_md2_interface_age)])
CLUTTER_MD2_LT_CURRENT=lt_current
CLUTTER_MD2_LT_REV=lt_revision
CLUTTER_MD2_LT_AGE=lt_age
CLUTTER_MD2_LT_VERSION="$CLUTTER_MD2_LT_CURRENT:$CLUTTER_MD2_LT_REV:$CLUTTER_MD2_LT_AGE"
CLUTTER_MD2_LT_LDFLAGS="-version-info $CLUTTER_MD2_LT_VERSION"

AC_SUBST(CLUTTER_MD2_LT_VERSION)
AC_SUBST(CLUTTER_MD2_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Make libtool respect the make -s switch (kudos to jacob berman)
changequote(,)dnl
LIBTOOL="${LIBTOOL} \$(shell echo \"\$(MFLAGS)\" | awk '/^[^ ]*s/ { print \"--silent\" }')"
changequote([,])dnl

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.

dnl ========================================================================

CLUTTER_MD2_REQUIRES="clutter-0.7 glib-2.0 >= 2.14 gobject-2.0"

PKG_CHECK_MODULES(CLUTTER_MD2_DEPS, [$CLUTTER_MD2_REQUIRES])

AC_SUBST(CLUTTER_MD2_REQUIRES)

dnl ========================================================================

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

dnl ========================================================================

AC_SUBST(GCC_FLAGS)

CLUTTER_MD2_CFLAGS="$CLUTTER_MD2_DEPS_CFLAGS"
CLUTTER_MD2_LIBS="$CLUTTER_MD2_DEPS_LIBS"

AC_SUBST(CLUTTER_MD2_CFLAGS)
AC_SUBST(CLUTTER_MD2_LIBS)

AC_CONFIG_FILES([
        Makefile
        clutter-md2/Makefile
        clutter-md2/clutter-md2-version.h
        clutter-md2.pc
])

AC_OUTPUT
