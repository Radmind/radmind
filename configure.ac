
# Process this file with autoconf to produce a configure script.
AC_INIT(radmind, VERSION, radmind@umich.edu)
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright (c) 2003 Regents of The University of Michigan.  All Rights Reserved.])
AC_CONFIG_SRCDIR([applefile.c])

# Set up variables
AC_ARG_VAR(diffpath, [path to diff utility])

AC_ARG_WITH(server, AC_HELP_STRING([--with-server=SERVER], [default radmind server]), server="$withval", server="radmind" )
AC_SUBST(server) 

AC_ARG_WITH(server, AC_HELP_STRING([--with-authlevel=AUTHLEVEL], [default TLS authorization level]), authlevel="$withval", authlevel="0" )
AC_SUBST(authlevel) 

AC_ARG_WITH(radminddir, AC_HELP_STRING([--with-radminddir=DIR], [default radmind server storage]), radminddir="$withval", radminddir="/var/radmind" )
AC_SUBST(radminddir) 

AC_ARG_WITH(maxconnections, AC_HELP_STRING([--with-max-connections=MAX], [maximum number of simultaneous connections]), maxconnections="$withval", maxconnections=30 )
AC_SUBST(maxconnections) 

#AC_ARG_WITH(certdir, AC_HELP_STRING([--cert=DIR], [default certificate location]), certdir="$withval", certdir="/var/radmind/cert" )
#AC_SUBST(certdir) 

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(diffpath, diff)

# Checks for libraries.
AC_CHECK_LIB([nsl], [inet_ntoa])
AC_CHECK_LIB([socket], [socket])

CHECK_SSL
CHECK_PAM

# Miscellaneous:
if test x_$GCC = x_yes; then
    OPTOPTS=${OPTOPTS:-"-Wall -Wmissing-prototypes"}
fi
AC_SUBST(OPTOPTS)

AC_CONFIG_SUBDIRS(libsnet)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
