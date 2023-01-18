AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS     = test
test_SOURCES     = test.c
test_CPPFLAGS    = -DCONFIG_DIR=\"$(sysconfdir)\" -DLIBRARY_DIR=\"$(pkglibdir)\"
test_LDADD       = $(top_builddir)/librender.a -lgdi32

include_HEADERS  = texture.h display.h
noinst_HEADERS   = utils.h

lib_LTLIBRARIES      = librender.la
librender_la_SOURCES = utils.c texture.c display.c
librender_la_LDFLAGS = -no-undefined
librender_la_LIBADD  = -lgdi32

