AUTOMAKE_OPTIONS = foreign

libavdevincludedir = $(includedir)/libavdev
nobase_libavdevinclude_HEADERS = texture.h adev.h vdev.h
noinst_HEADERS = utils.h

lib_LTLIBRARIES = libavdev.la
libavdev_la_SOURCES = utils.c texture.c adev.c vdev.c
libavdev_la_LDFLAGS = -no-undefined

if WIN32
libavdev_la_LIBADD  = -lwinmm -lgdi32
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libavdev.pc

bin_PROGRAMS = test_adev test_vdev

test_adev_SOURCES  = test.c
test_adev_CFLAGS   = -D_TEST_ADEV_
test_adev_LDADD    = $(top_builddir)/libavdev.la $(libavdev_la_LIBADD)

test_vdev_SOURCES  = test.c
test_vdev_CPPFLAGS = -D_TEST_VDEV_
test_vdev_LDADD    = $(top_builddir)/libavdev.la $(libavdev_la_LIBADD)
