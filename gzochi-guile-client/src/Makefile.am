## Process this file with automake to produce Makefile.in
#
# Makefile.am: Automake input file.
#
# Copyright (C) 2015 Julian Graham
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this package.  If not, see <http://www.gnu.org/licenses/>.
#

# Don't rely on GUILE_SITE for this location; its value is set at the time that
# its `pkg-config' file is created, which makes it a poor fit for dynamic
# environments.

pkgguile_sitedir = ${datadir}/guile/site/${GUILE_EFFECTIVE_VERSION}

nobase_dist_pkgguile_site_DATA = gzochi/client.scm gzochi/client/protocol.scm \
	gzochi/main-loop.scm
