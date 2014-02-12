#!/bin/sh
# autogen.sh
#
# Copyright (C) 2014 Julian Graham
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

echo "Generating build files for AberMUD example game client."

BASEDIR=`dirname $0`
OLDDIR=$PWD

cd $BASEDIR

# Generate build files.

aclocal
if [ $? != 0 ]; then
    cd $OLDDIR
    exit 1
fi

automake -a
if [ $? != 0 ]; then
    cd $OLDDIR
    exit 1
fi

autoconf
if [ $? != 0 ]; then
    cd $OLDDIR
    exit 1
fi

cd $OLDDIR
