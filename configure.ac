#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([libavdev], [1.0.0], [chenk@apical.com.cn])
AC_CONFIG_SRCDIR([test.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([winmm], [waveOutOpen])
AC_CHECK_LIB([gdi32], [CreateWindow])

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
AC_CHECK_FUNCS([clock_gettime strcasecmp])

AC_OUTPUT
