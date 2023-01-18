AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS     = test
test_SOURCES     = test.c
test_CPPFLAGS    = -DCONFIG_DIR=\"$(sysconfdir)\" -DLIBRARY_DIR=\"$(pkglibdir)\"
test_LDADD       = $(top_builddir)/librender.la -lgdi32

noinst_HEADERS   = utils.h
include_HEADERS  = texture.h vdev.h

lib_LTLIBRARIES      = librender.la
librender_la_SOURCES = utils.c texture.c vdev.c
librender_la_LDFLAGS = -no-undefined
librender_la_LIBADD  = -lgdi32
