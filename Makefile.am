AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = test_adev test_vdev

test_adev_SOURCES  = test.c
test_adev_CPPFLAGS = -D_TEST_ADEV_ -DCONFIG_DIR=\"$(sysconfdir)\" -DLIBRARY_DIR=\"$(pkglibdir)\"
test_adev_LDADD    = $(top_builddir)/librender.la -lwinmm -lgdi32

test_vdev_SOURCES  = test.c
test_vdev_CPPFLAGS = -D_TEST_VDEV_ -DCONFIG_DIR=\"$(sysconfdir)\" -DLIBRARY_DIR=\"$(pkglibdir)\"
test_vdev_LDADD    = $(top_builddir)/librender.la -lwinmm -lgdi32

noinst_HEADERS  = utils.h
include_HEADERS = texture.h adev.h vdev.h

lib_LTLIBRARIES      = librender.la
librender_la_SOURCES = utils.c texture.c adev.c vdev.c
librender_la_LDFLAGS = -no-undefined
librender_la_LIBADD  = -lwinmm -lgdi32
