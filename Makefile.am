AUTOMAKE_OPTIONS = foreign

libavdevincludedir = $(includedir)/libavdev
nobase_libavdevinclude_HEADERS = texture.h adev.h vdev.h
noinst_HEADERS = utils.h

lib_LTLIBRARIES = libavdev.la
libavdev_la_SOURCES = utils.c texture.c
libavdev_la_LDFLAGS = -no-undefined

CFLAGS += -Wall -Werror

if WIN32
CFLAGS += -DWIN32
libavdev_la_SOURCES += adev-win32.c vdev-win32.c
libavdev_la_LIBADD = -lwinmm -lgdi32
endif

if LINUX
libavdev_la_SOURCES += adev-linux.c vdev-linux.c
CFLAGS += -DLINUX
libavdev_la_LIBADD = -lm
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
