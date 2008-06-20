#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="clutter-md2"

which gnome-autogen.sh || {
        echo "Missing gnome-autogen.sh: you need to install gnome-common"
        exit 1
}

. gnome-autogen.sh
