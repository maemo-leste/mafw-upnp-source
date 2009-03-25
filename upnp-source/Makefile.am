#
# Makefile.am for MAFW uPnP source library.
#
## Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

mafwext_LTLIBRARIES		= mafw-upnp-source.la

mafw_upnp_source_la_CPPFLAGS	= $(DEPS_CFLAGS) $(_CFLAGS)
mafw_upnp_source_la_LIBADD	= $(DEPS_LIBS)
if HAVE_CONIC
mafw_upnp_source_la_CPPFLAGS	+= -DHAVE_CONIC $(LIBCONIC_CFLAGS)
mafw_upnp_source_la_LIBADD	+= $(LIBCONIC_LIBS) 
endif
mafw_upnp_source_la_LDFLAGS 	= -module -avoid-version $(_LDFLAGS)

noinst_HEADERS			= mafw-upnp-source.h \
				  mafw-upnp-source-didl.h

mafw_upnp_source_la_SOURCES	= mafw-upnp-source.c \
				  mafw-upnp-source.h \
				  mafw-upnp-source-didl.c \
				  mafw-upnp-source-didl.h \
				  mafw-upnp-source-util.c \
				  mafw-upnp-source-util.h

mafwextdir			= $(plugindir)

CLEANFILES			= *.gcno *.gcda
MAINTAINERCLEANFILES		= Makefile.in
