
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
AC_CHECK_SIZEOF(time_t)

# Checks for header files

# glibc 2.25 still includes sys/sysmacros.h in sys/types.h but emits
# deprecation warning which causes compilation failure later with -Werror.
# So use -Werror here as well to force proper sys/sysmacros.h detection.
SAVED_CFLAGS="$CFLAGS"
CFLAGS="$HOST_CFLAGS -Werror"
AC_HEADER_MAJOR
CFLAGS="$SAVED_CFLAGS"

# Enable features.

AC_ARG_ENABLE([pam],
    AS_HELP_STRING([--enable-pam], [Build with pam @<:@default=yes@:>@]),
    [AS_CASE(${enableval}, [yes], [], [no], [],
        [AC_MSG_ERROR([bad value '${enableval}' for --enable-pam])])],
    [enable_pam=check])

AC_ARG_ENABLE([ssl],
    AS_HELP_STRING([--enable-ssl], [Build with openssl @<:@default=yes@:>@]),
    [AS_CASE(${enableval}, [yes], [], [no], [],
        [AC_MSG_ERROR([bad value '${enableval}' for --enable-ssl])])],
    [enable_ssl=check])

AC_ARG_ENABLE([zlib],
    AS_HELP_STRING([--enable-zlib], [Build with zlib @<:@default=yes@:>@]),
    [AS_CASE(${enableval}, [yes], [], [no], [],
        [AC_MSG_ERROR([bad value '${enableval}' for --enable-zlib])])],
    [enable_zlib=check])

# Checks for libraries.
AC_CHECK_LIB(c, inet_aton, libc_inet_aton=yes) 
if test x$libc_inet_aton != xyes; then 
    AC_CHECK_LIB(resolv, inet_aton) 
fi 
if test x$host_vendor = xapply; then
    AC_CHECK_LIB(c, lchown, [AC_DEFINE([HAVE_LCHOWN], [], [lchown])])
    AC_CHECK_LIB(c, lchmod, [AC_DEFINE([HAVE_LCHMOD], [], [lchmod])])
fi
AC_CHECK_LIB(nsl, gethostbyaddr)
AC_CHECK_LIB([socket], [socket])

# Checks for pam.
AS_IF([test x$enable_pam != xno],
    [PKG_CHECK_MODULES([pam], [pam],
	[AC_CHECK_HEADERS([pam/pam_appl.h],
	    [], [AC_CHECK_HEADERS([security/pam_appl.h])])]
        [AC_DEFINE([HAVE_LIBPAM], [1], [libpam])],
        [AS_IF([test x$enable_pam != xyes],
            [AC_MSG_WARN([cannot find pam.pc, disabling pam support])],
            [enable_pam=error])
        ])
    ])

AS_IF([test x$enable_pam = xerror],
    [AC_MSG_ERROR([cannot find pam.pc and pam support requested])])

AM_CONDITIONAL([ENABLE_PAM], [test x$enable_pam = xyes])
AC_SUBST([pam_CFLAGS])
AC_SUBST([pam_LIBS])

# Checks for openssl.
AS_IF([test x$enable_ssl != xno],
    [PKG_CHECK_MODULES([ssl], [openssl],
        [AC_DEFINE([HAVE_LIBSSL], [1], [libssl])],
        [AS_IF([test x$enable_ssl != xyes],
            [AC_MSG_WARN([cannot find openssl.pc, disabling libssl support])],
            [enable_ssl=error])
        ])
    ])

AC_CHECK_HEADER([openssl/ssl.h], [], [AC_MSG_ERROR([header file <openssl/ssl.h>
is required for this software.  You may be running RedHat 9.  If so, see the FAQ or the README for further instructions.])])
AC_CHECK_LIB([crypto], [X509_VERIFY_PARAM_set_flags], [AC_DEFINE([HAVE_X509_VERIFY_PARAM], [], [X509_verify_param])])

AS_IF([test x$enable_ssl = xerror],
    [AC_MSG_ERROR([cannot find openssl.pc and libssl support requested])])

AM_CONDITIONAL([ENABLE_LIBSSL], [test x$enable_ssl = xyes])
AC_SUBST([ssl_CFLAGS])
AC_SUBST([ssl_LIBS])

AC_CHECK_HEADER([dns_sd.h], [AC_DEFINE([HAVE_DNSSD], [], [dnssd])])
AC_CHECK_LIB(dns_sd, DNSServiceRegister)

# Checks for zlib.
AS_IF([test x$enable_zlib != xno],
    [PKG_CHECK_MODULES([zlib], [zlib],
        [AC_DEFINE([HAVE_ZLIB], [1], [zlib])],
        [AS_IF([test x$enable_zlib != xyes],
            [AC_MSG_WARN([cannot find zlib.pc, disabling zlib support])],
            [enable_zlib=error])
        ])
    ])

AS_IF([test x$enable_zlib = xerror],
    [AC_MSG_ERROR([cannot find zlib.pc and zlib support requested])])

AM_CONDITIONAL([ENABLE_ZLIB], [test x$enable_zlib = xyes])
AC_SUBST([zlib_CFLAGS])
AC_SUBST([zlib_LIBS])

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

ac_configure_args="$ac_configure_args --enable-sasl=no";
AC_MSG_RESULT([Disabling SASL for libsnet])

AC_CONFIG_SUBDIRS(libsnet)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
