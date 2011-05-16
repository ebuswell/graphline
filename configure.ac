# configure.ac
# 
# Process this file with autoconf to produce a configure script.
#
# Copyright 2011 Evan Buswell
# 
# This file is part of Graphline.
# 
# Graphline is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# 
# Graphline is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with Graphline.  If not, see <http://www.gnu.org/licenses/>.

# define version information
m4_define(GRAPHLINE_VERSION, [0.1])

m4_define(GRAPHLINE_INTERFACE_REVISION, 0)
m4_define(GRAPHLINE_INTERFACE_BACK_COMPATABILITY, 0)
m4_define(GRAPHLINE_SOURCE_REVISION, 0)


AC_PREREQ([2.65])
AC_INIT([graphline], GRAPHLINE_VERSION, [ebuswell@gmail.com])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR([src/graphline.c])
AC_CONFIG_HEADERS([src/config.h])

VERSION_INFO=GRAPHLINE_INTERFACE_REVISION:GRAPHLINE_SOURCE_REVISION:GRAPHLINE_INTERFACE_BACK_COMPATABILITY
AC_SUBST(VERSION_INFO)

[GRAPHLINE_VERSION]=GRAPHLINE_VERSION
AC_SUBST([GRAPHLINE_VERSION])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES([GLIB2], [glib-2.0], [HAVE_GLIB=true], [AC_MSG_ERROR([glib is required to build AC_PACKAGE_NAME])])
PKG_CHECK_MODULES([ATOMICKIT], [atomickit], [HAVE_ATOMICKIT=true], [AC_MSG_ERROR([atomickit is required to build AC_PACKAGE_NAME])])

# Checks for language features.
AC_LANG([C])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile graphline.pc])

AC_OUTPUT
