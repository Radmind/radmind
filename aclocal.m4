AC_DEFUN([CHECK_SNET],
[
    AC_MSG_CHECKING(for snet)
    snetdir="libsnet"
    AC_ARG_WITH(snet,
	    AC_HELP_STRING([--with-snet=DIR], [path to snet]),
	    snetdir="$withval")
    if test -f "$snetdir/snet.h"; then
	found_snet="yes";
	CPPFLAGS="$CPPFLAGS -I$snetdir";
    fi
    if test x_$found_snet != x_yes; then
	AC_MSG_ERROR(cannot find snet libraries)
    else
	LIBS="$LIBS -lsnet";
	LDFLAGS="$LDFLAGS -L$snetdir";
    fi
    AC_MSG_RESULT(yes)
])

AC_DEFUN([CHECK_SSL],
[
    AC_MSG_CHECKING(for ssl)
    ssldirs="/usr/local/openssl /usr/lib/openssl /usr/openssl \
	    /usr/local/ssl /usr/lib/ssl /usr/ssl \
	    /usr/pkg /usr/local /usr"
    AC_ARG_WITH(ssl,
	    AC_HELP_STRING([--with-ssl=DIR], [path to ssl]),
	    ssldirs="$withval")
    for dir in $ssldirs; do
	ssldir="$dir"
	if test -f "$dir/include/openssl/ssl.h"; then
	    found_ssl="yes";
	    CPPFLAGS="$CPPFLAGS -I$ssldir/include";
	    break;
	fi
	if test -f "$dir/include/ssl.h"; then
	    found_ssl="yes";
	    CPPFLAGS="$CPPFLAGS -I$ssldir/include";
	    break
	fi
    done
    if test x_$found_ssl != x_yes; then
	AC_MSG_ERROR(cannot find ssl libraries)
    else
	AC_DEFINE(HAVE_LIBSSL)
	LIBS="$LIBS -lssl -lcrypto";
	LDFLAGS="$LDFLAGS -L$ssldir/lib";
    fi
    AC_MSG_RESULT(yes)
])

AC_DEFUN([CHECK_ZEROCONF],
[
    AC_MSG_CHECKING(for zeroconf)
    zeroconfdirs="/usr /usr/local"
    AC_ARG_WITH(zeroconf,
	    AC_HELP_STRING([--with-zeroconf=DIR], [path to zeroconf]),
	    zeroconfdirs="$withval")
    for dir in $zeroconfdirs; do
	zcdir="$dir"
	if test -f "$dir/include/DNSServiceDiscovery/DNSServiceDiscovery.h"; then
	    found_zeroconf="yes";
	    CPPFLAGS="$CPPFLAGS -I$zcdir/include";
	    break;
	fi
    done
    if test x_$found_zeroconf != x_yes; then
	AC_MSG_RESULT(no)
    else
	AC_DEFINE(HAVE_ZEROCONF)
	AC_MSG_RESULT(yes)
    fi
])
