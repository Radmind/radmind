#!/bin/sh
#
# Prepare a checked-out source tree for build
#
# Make sure git and autoconf are in PATH

# If necessary, populate libsnet/
if [ -f "./.gitmodules" -a ! -f "./libsnet/.git" ]
then
    git submodule init
    git submodule update
fi

(cd libsnet && autoconf)

autoconf

exit 0
