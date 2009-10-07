#
# Makefile.am for MAFW uPnP source library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

TESTS				= test-upnp-source test-didl test-util

check_PROGRAMS			= $(TESTS)
noinst_PROGRAMS			= $(TESTS)

LDADD				= $(CHECKMORE_LIBS) \
				  $(DEPS_LIBS) \
				  $(MAEMO_LIBS) \
				  $(top_builddir)/upnp-source/mafw-upnp-source.la
AM_LDFLAGS			= $(_LDFLAGS)
AM_CPPFLAGS			= $(CHECKMORE_CFLAGS) \
				  $(DEPS_CFLAGS) \
				  $(MAEMO_CFLAGS) \
				  -I$(top_srcdir)
if HAVE_CONIC
AM_CPPFLAGS			+= -DHAVE_CONIC
endif
CLEANFILES			= $(BUILT_SOURCES) $(TESTS) *.gcno *.gcda 
DISTCLEANFILES			= $(BUILT_SOURCES) $(TESTS)
MAINTAINERCLEANFILES		= Makefile.in $(BUILT_SOURCES) $(TESTS)


if ENABLE_SAMPLES

upnp_source_tt_SOURCES		= upnp-source-tt.c

upnp_source_tt_CPPFLAGS		= $(DEPS_CFLAGS) \
				  $(MAEMO_CFLAGS) \
				  $(MAFW_SHARED_CFLAGS) \
				  -I$(top_srcdir)

upnp_source_tt_LDADD		= $(DEPS_LIBS) \
				  $(MAEMO_LIBS) \
				  $(MAFW_SHARED_LIBS) \
				  $(top_builddir)/upnp-source/mafw-upnp-source.la

noinst_PROGRAMS			+= upnp-source-tt

CLEANFILES			+= upnp-source-tt

endif

# Run valgrind on tests.
VG_OPTS				:= --leak-check=full --show-reachable=yes --suppressions=test.suppressions
vg: $(TESTS)
	for p in $^; do \
		G_DEBUG='always-malloc' \
		libtool --mode=execute valgrind $(VG_OPTS) $$p 2>vglog.$$p; \
	done;
	-rm -f vgcore.*
