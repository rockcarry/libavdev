#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libavdev], [1.1.0], [chenk@apical.com.cn])

AC_CONFIG_SRCDIR([test.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile libavdev.pc])

AM_INIT_AUTOMAKE([subdir-objects])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for pkg-config
PKG_PROG_PKG_CONFIG

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime memset])

# Checks for Optional Features
AC_ARG_ENABLE(debug, [AS_HELP_STRING([--enable-debug], [enable debug mode [default=no]])], [], [debug=no])
AM_CONDITIONAL([ENABLE_DEBUG], [test x"$enable_debug" = x"yes"])

AC_ARG_ENABLE(tests, [AS_HELP_STRING([--enable-tests], [enable test programs [default=no]])], [], [tests=no])
AM_CONDITIONAL([ENABLE_TESTS], [test x"$enable_tests" = x"yes"])

# Checks for build & host enviroment
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
case $host_os in
  *cygwin* | *mingw*)
    win32="yes"
    AC_CHECK_LIB([winmm], [waveOutOpen])
    AC_CHECK_LIB([gdi32], [CreateWindow])
  ;;
  *linux*)
    linux="yes"
  ;;
esac
AM_CONDITIONAL([WIN32], [test "x$win32" = "xyes"])
AM_CONDITIONAL([LINUX], [test "x$linux" = "xyes"])

AC_OUTPUT
