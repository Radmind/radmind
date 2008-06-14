
# Process this file with autoconf to produce a configure script.
AC_INIT(radmind, VERSION, radmind@umich.edu)
AC_CONFIG_HEADER(config.h)
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright (c) 2003 Regents of The University of Michigan.  All Rights Reserved.])
AC_CONFIG_SRCDIR([applefile.c])

# Set up variables
AC_ARG_VAR(diffpath, [path to diff utility])
AC_ARG_VAR(echopath, [path to echo utility])

AC_ARG_WITH(server, AC_HELP_STRING([--with-server=SERVER], [default radmind server]), server="$withval", server="radmind" )
AC_SUBST(server) 

AC_ARG_WITH(authlevel, AC_HELP_STRING([--with-authlevel=AUTHLEVEL], [default TLS authorization level]), authlevel="$withval", authlevel="0" )
AC_SUBST(authlevel) 

AC_ARG_WITH(radminddir, AC_HELP_STRING([--with-radminddir=DIR], [default radmind server storage]), radminddir="$withval", radminddir="/var/radmind" )
AC_SUBST(radminddir) 

AC_ARG_WITH(maxconnections, AC_HELP_STRING([--with-max-connections=MAX], [maximum number of simultaneous connections]), maxconnections="$withval", maxconnections=0 )
AC_SUBST(maxconnections) 

AC_ARG_WITH(maildomain, AC_HELP_STRING([--with-maildomain=DOMAIN], [default radmind mail domain]), maildomain="$withval", maildomain="" )
AC_SUBST(maildomain) 

build_date=`date "+%B %d, %Y"`
AC_SUBST(build_date) 

#AC_ARG_WITH(certdir, AC_HELP_STRING([--cert=DIR], [default certificate location]), certdir="$withval", certdir="/var/radmind/cert" )
#AC_SUBST(certdir) 

AC_CANONICAL_SYSTEM

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(diffpath, diff)
AC_PATH_PROG(echopath, echo)
AC_PATH_PROG(mktemppath, mktemp)
AC_SYS_LARGEFILE

# Check sizes
AC_CHECK_SIZEOF(off_t)

# Checks for libraries.
AC_CHECK_LIB(c, inet_aton, libc_inet_aton=yes) 
if test x$libc_inet_aton != xyes; then 
    AC_CHECK_LIB(resolv, inet_aton) 
fi 
AC_CHECK_LIB(c, lchown)
AC_CHECK_LIB(c, lchmod)
AC_CHECK_LIB(nsl, gethostbyaddr)
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([ssl], [SSL_accept], , [CHECK_SSL])
AC_CHECK_HEADER([openssl/ssl.h], [], [AC_MSG_ERROR([header file <openssl/ssl.h>
is required for this software.  You may be running RedHat 9.  If so, see the FAQ or the README for further instructions.])])
AC_CHECK_LIB([crypto], [SSLeay_version], , [CHECK_SSL])
AC_CHECK_LIB([pam], [pam_start],
    [
    AC_CHECK_HEADERS(pam/pam_appl.h, , [AC_CHECK_HEADERS(security/pam_appl.h)])
    AC_DEFINE(HAVE_LIBPAM)
    LIBS="$LIBS -lpam";
    ] 
)
AC_CHECK_HEADER([dns_sd.h], [AC_DEFINE(HAVE_DNSSD)], [], [])
AC_CHECK_LIB(dns_sd, DNSServiceRegister)

CHECK_ZLIB
CHECK_UNIVERSAL_BINARIES

# HPUX lacks wait4 and strtoll
AC_CHECK_FUNCS(wait4 strtoll)

# Miscellaneous:
if test x_"$OPTOPTS" = x_; then
    if test x_$GCC = x_yes; then
        OPTOPTS="$OPTOPTS -Wall -Wmissing-prototypes"
    fi
    if test x_$build_vendor = x_hp; then
        OPTOPTS="$OPTOPTS -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1"
    fi
fi
AC_SUBST(OPTOPTS)

SET_NO_SASL
AC_CONFIG_SUBDIRS(libsnet)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
