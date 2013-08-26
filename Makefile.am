# Makefile.am
# 
# Copyright 2013 Evan Buswell
# 
# This file is part of Graphline.
# 
# Graphline is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 2.
# 
# Graphline is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with Graphline.  If not, see <http://www.gnu.org/licenses/>.

MAINTAINERCLEANFILES = aclocal.m4 config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing src/config.h.in~ compile
EXTRA_DIST = autogen.sh Doxyfile graphline.pc.in
AUTOMAKE_OPTIONS = foreign serial-tests subdir-objects

lib_LTLIBRARIES = libgraphline.la
libgraphline_la_SOURCES = src/graphline.c
libgraphline_la_LIBADD = $(ATOMICKIT_LIBS)
libgraphline_la_LDFLAGS = $(LDFLAGS) -L$(libdir) -export-dynamic -version_info @VERSION_INFO@
libgraphline_la_CFLAGS = $(CFLAGS) $(ATOMICKIT_CFLAGS) -fplan9-extensions -Og -g3 -Wall -Wextra -Wmissing-prototypes -Wredundant-decls -Iinclude
libgraphlineincludedir = $(includedir)
libgraphlineinclude_HEADERS = include/graphline.h

pkgconfigdir = $(libdir)/pkgconfig
nodist_pkgconfig_DATA = graphline.pc

check_PROGRAMS = test
test_SOURCES = src/test.c
test_LDADD = $(ATOMICKIT_LIBS) -lgraphline
test_LDFLAGS = $(LD_FLAGS) -rpath $(libdir)
test_CFLAGS = $(CFLAGS) $(ATOMICKIT_CFLAGS) -fplan9-extensions -Og -g3 -Wall -Wextra -Wmissing-prototypes -Wredundant-decls -Iinclude
TESTS = test
