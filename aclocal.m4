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
	HAVE_SNET=yes
    fi
    AC_SUBST(HAVE_SNET)
    AC_MSG_RESULT(yes)
])

AC_DEFUN([CHECK_PAM],
[
    AC_MSG_CHECKING(for pam)
    pamdirs="/usr/local /usr"
    AC_ARG_WITH(pam,
	    AC_HELP_STRING([--with-pam=DIR], [path to pam]),
	    pamdirs="$withval")
    for dir in $pamdirs; do
	pamdir="$dir"
	if test -f "$dir/include/security/pam_appl.h"; then
	    found_pam="yes";
	    CPPFLAGS="$CPPFLAGS -I$pamdir/include/security";
	    break;
	fi
	if test -f "$dir/include/pam/pam_appl.h"; then
	    found_pam="yes";
	    CPPFLAGS="$CPPFLAGS -I$pamdir/include/pam";
	    break
	fi
    done
    if test x_$found_pam != x_yes; then
	HAVE_PAM=no
	AC_MSG_ERROR(cannot find pam headers)
    else
	PAMDEFS=-DPAM;
	AC_SUBST(PAMDEFS)
	HAVE_PAM=yes
    fi
    AC_SUBST(HAVE_PAM)
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
	TLSDEFS=-DTLS;
	AC_SUBST(TLSDEFS)
	LIBS="$LIBS -lssl -lcrypto";
	LDFLAGS="$LDFLAGS -L$ssldir/lib";
	HAVE_SSL=yes
    fi
    AC_SUBST(HAVE_SSL)
    AC_MSG_RESULT(yes)
])
