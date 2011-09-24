#!/bin/sh
#
# Copyright (c) 2004, 2007 Regents of The University of Michigan.
# All Rights Reserved.  See COPYRIGHT.
#
# Radmind Assistant shell script (rash)
# Basic client side functions include:
#
#	trip
#	update
#	create
#	auto
#	force
#

# Command line options:
# -c  without checksums

# Config file stuff:
#
# server
# -w # -x, -y, -z
# lcreate -l -U
# -c sha1

KFILE="_RADMIND_COMMANDFILE"
SERVER="_RADMIND_HOST"
TLSLEVEL="_RADMIND_AUTHLEVEL"
PORT="6222"
EDITOR=${EDITOR:-vi}
PAGER=${PAGER:-cat}
USER=${SUDO_USER:-$USER}
TMPDIR="${TMPDIR:=/tmp}"
DEFAULTS="/etc/defaults/radmind"
FSDIFFROOT="."
DEFAULTWORKDIR="/"
FLAG="_RADMIND_DIR/client/.RadmindRunning"
CHECKEDOUT="_RADMIND_DIR/client/.CheckedOut"
MAILDOMAIN="_RADMIND_MAIL_DOMAIN"
ECHO="_RADMIND_ECHO_PATH"
VERSION=_RADMIND_VERSION

# variable containing all network-related flags (-w, -h, -p, etc.) 
NETOPTS=

PREAPPLY="_RADMIND_PREAPPLY"
POSTAPPLY="_RADMIND_POSTAPPLY"

PATH=/usr/local/bin:/usr/bin:/bin; export PATH
RETRY=10

MKTEMP="_RADMIND_MKTEMP"
TEMPFILES=FALSE
RASHTMP="${TMPDIR}/.ra.$$"
if [ -f "${MKTEMP}"  ]; then
    RASHTMP=`${MKTEMP} -qd "${TMPDIR}/.ra.$$.XXXXXX"`
    if [ $? -ne 0 ]; then
	$ECHO "mktemp failed"
	exit 1
    fi
fi
LTMP="${RASHTMP}/lapply.out"
FTMP="${RASHTMP}/fsdiff.out"

# different systems use different default dirs
if [ ! -f "${DEFAULTS}" ]; then
    DEFAULTS="/etc/default/radmind"
    if [ ! -f "${DEFAULTS}" ]; then
	DEFAULTS="/etc/radmind.defaults"
    fi
fi

Yn() {
    $ECHO -n "$*" "[Yn] "
    read ans
    if [ $? -ne 0 ]; then
	return 0
    fi
    if [ -z "$ans" -o X"$ans" = Xy -o X"$ans" = XY -o X"$ans" = Xyes ]; then
	return 1
    fi
    return 0
}

checkedout() {
    if [ -s ${CHECKEDOUT} ]; then
	OWNER=`cat ${CHECKEDOUT}`
	return 1
    fi
    return 0
}

usage() {
    $ECHO "Usage:	$0 [ -cIltV ] [ -% | -q ] [ -C \"generate\" | -C <checksum> ] [ -D working-directory ] [ -h server ] [ -p port ] [ -w authlevel ] { trip | update | create | auto | force | checkout | checkin } [ /path/or/file ]" >&2
    exit 1
}

cleanup() {
    if [ "$TEMPFILES" = FALSE ]; then
	rm -fr "${RASHTMP}"
    fi
}

cleanup_and_exit() {
    cleanup
    exit 1
}

dopreapply() {
    if [ -d ${PREAPPLY} ]; then
	SCRIPTS=`find ${PREAPPLY} -perm -u+x \! -type d | sort`
	if [ "${SCRIPTS}" ]; then
	    for script in ${SCRIPTS}; do
		${script} "$1"
	    done
	fi
    fi
}

dopostapply() {
    if [ -d ${POSTAPPLY} ]; then
	SCRIPTS=`find ${POSTAPPLY} -perm -u+x \! -type d | sort`
	if [ "${SCRIPTS}" ]; then
	    for script in ${SCRIPTS}; do
		${script} "$1"
	    done
	fi
    fi
}

cksum_generate() {
    if [ -n "$FSDIFF_CHECKSUM" ]; then
	FTMP_CHECKSUM=`openssl sha1 ${FTMP} | awk '{ print $2 }'`
    fi
}

cksum_compare() {
    if [ -n "$FSDIFF_CHECKSUM" -a \
	    "$FSDIFF_CHECKSUM" != "generate" -a \
	    "$FSDIFF_CHECKSUM" != "$FTMP_CHECKSUM" ]; then
	return 1
    fi

    return 0
}

cksum_print() {
    if [ -n "$FSDIFF_CHECKSUM" ]; then
	$ECHO "Difference transcript checksum: $FTMP_CHECKSUM"
    fi
}

cksum_mismatch() {
    if [ -n "$FSDIFF_CHECKSUM" ]; then
	$ECHO
	$ECHO "**** Difference transcript checksum mismatch!"
	$ECHO "****	Expected: $FSDIFF_CHECKSUM"
	$ECHO "****	Actual:   $FTMP_CHECKSUM"
	#ECHO
    fi
}

update() {
    opt="$1"
    kopt=
    apply=ask
    can_edit=no

    checkedout
    if [ $? -eq 1 ]; then
	$ECHO "Checked out by ${OWNER}"
	if [ x"$opt" = x"interactive" -a x"$USER" = x"$OWNER" ]; then
	    Yn "Continue with update?"
	    if [ $? -eq 0 ]; then
		exit 1
	    fi
	else
	    exit 1
	fi
    fi

    if [ x"$opt" = x"interactive" ]; then
	kopt="-n"
    fi

    ktcheck ${kopt} ${NETOPTS} -c sha1
    case "$?" in
    0)  
	if [ x"$opt" = x"hook" -a ! -f "${FLAG}" ]; then
	    cleanup
	    exit 0
	fi
	;;

    1)
	if [ x"$opt" = x"interactive" ]; then
	    Yn "Update command file and/or transcripts?"
	    if [ $? -eq 1 ]; then
		ktcheck ${NETOPTS} -c sha1
		RC=$?
		if [ $RC -ne 1 ]; then
		    $ECHO Nothing to update
		    cleanup
		    exit $RC
		fi
	    fi
	fi
	;;

    *)	
	cleanup
    	exit $?
	;;
    esac

    fsdiff -A ${CASE} ${FPROGRESS} ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1
    fi

    if [ ! -s ${FTMP} ]; then
	$ECHO Nothing to apply.
	cleanup
	exit 0
    fi
    if [ x"$opt" = x"interactive" ]; then
	${PAGER} ${FTMP}
	infocmp >/dev/null 2>&1
	if [ $? -eq 0 ]; then
	    can_edit=yes
	fi
    fi
    
    if [ x"$opt" = x"interactive" -a -d "${PREAPPLY}" \
		-a ! -z "`ls ${PREAPPLY} 2>/dev/null`" ]; then
	Yn "Run pre-apply scripts on difference transcript?"
        if [ $? -eq 1 ]; then
            dopreapply ${FTMP}
        fi
    elif [ x"$opt" != x"interactive" ]; then
	dopreapply ${FTMP}
    fi
    if [ x"${opt}" = x"interactive" ]; then
	while [ 1 ]; do
	    cksum_generate
	    if ! cksum_compare; then
		cksum_mismatch
	    fi

	    if [ x"${can_edit}" = x"yes" ]; then
		$ECHO -n "(e)dit difference transcript, "
	    fi
	    $ECHO -n "(a)pply or (c)ancel? "

	    read ans
	    if [ $? -ne 0 ]; then
		cleanup_and_exit
	    fi

	    case "${ans}" in
	    a|A)
		break
		;;

	    c|C)
		$ECHO
		$ECHO Update cancelled
		cleanup
		exit 0
		;;

	    e|E)
		if [ x"${can_edit}" = x"yes" ]; then
		    ${EDITOR} ${FTMP}
		fi
		;;

	    *)
		;;

	    esac
	done
    fi
		
    lapply ${CASE} ${PROGRESS} ${NETOPTS} ${CHECKSUM} ${FTMP}
    case "$?" in
    0)	cksum_print
	;;

    *)  if [ x"$opt" = x"hook" ]; then
	    $ECHO -n "Applying changes failed, trying again "
	    $ECHO "in ${RETRY} seconds..."
	    sleep ${RETRY}
	    RETRY=${RETRY}0

	    $ECHO %OPENDRAWER
	    $ECHO %BEGINPOLE
    	else 
	    cleanup
	fi
	return 1
	;;
    esac
    if [ x"$opt" = x"interactive" -a -d "${POSTAPPLY}" \
		-a ! -z "`ls ${POSTAPPLY} 2>/dev/null`" ]; then
	Yn "Run post-apply scripts on difference transcript?"
        if [ $? -eq 1 ]; then
            dopostapply ${FTMP}
        fi
    elif [ x"$opt" != x"interactive" ]; then
	dopostapply ${FTMP}
    fi
    
    cleanup
}

# if a radmind defaults file exists, source it.
# options in the defaults file can be overridden
# with the options below.
if [ -f "${DEFAULTS}" ]; then
    . "${DEFAULTS}"
fi

while getopts %C:cD:h:Ilp:qr:tU:Vw: opt; do
    case $opt in
    %)  PROGRESS="-%"
	FPROGRESS="-%"
	;;

    q)  PROGRESS="-q"
	;;

    C)	FSDIFF_CHECKSUM="$OPTARG"
	if ! type openssl >/dev/null 2>&1; then
	    $ECHO "-C requires openssl, but no openssl found in PATH $PATH"
	    cleanup_and_exit
	fi
	;;

    c)	CHECKSUM="-csha1"
	;;

    D)  WORKDIR="$OPTARG"
	;;

    h)	SERVER="$OPTARG"
    	;;

    I)	CASE="-I"
	;;

    l)  USERAUTH="-l"
	;;

    p)  PORT="$OPTARG"
	;;

    r)  FSDIFFROOT="$OPTARG"
	;;

    t)	TEMPFILES="TRUE"
    	;;

    U)	USER="$OPTARG"
	USERNAME="$OPTARG"
    	;;

    V)	$ECHO ${VERSION}
	exit 0
	;;

    w)	TLSLEVEL="$OPTARG"
    	;;

    *)  usage
	;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -eq 2 ]; then
    FSDIFFROOT=$2
elif [ $# -ne 1 ]; then
    usage
fi

cd "${WORKDIR:-$DEFAULTWORKDIR}"

if [ ! -d "${RASHTMP}" ]; then
    mkdir -m 700 "${RASHTMP}"
    if [ $? -ne 0 ]; then
        $ECHO "Cannot create temporary directory $RASHTMP" 
	exit 1
    fi
fi

# Trap meaningful signals
trap cleanup_and_exit HUP INT PIPE QUIT TERM TRAP XCPU XFSZ

# combine network-related flags for ktcheck, lcreate, lapply
NETOPTS="-w ${TLSLEVEL} -h ${SERVER} -p ${PORT}"

case "$1" in
checkout)
    if [ ${USER} = root ]; then
	$ECHO -n "Username? [root] "
	read ans
	USER=${ans:-root}
    fi
    checkedout
    if [ $? -eq 1 ]; then
	$ECHO "Already checked out by ${OWNER}"
	if [ x${OWNER} = x${USER} ]; then
	    exit 1
	fi
	Yn "Force checkout?"
	if [ $? -eq 0 ]; then
	    exit 1
	fi
	$ECHO ${USER} has removed your checkout on `hostname` | mail -s `hostname`": Checkout broken" ${OWNER}@${MAILDOMAIN:-`hostname`}
    fi
    $ECHO ${USER} > ${CHECKEDOUT}
    ;;

checkin)
    checkedout
    if [ $? -eq 0 ]; then
	$ECHO "Not checked out"
	exit 1
    fi
    if [ ${USER} = root ]; then
	$ECHO -n "Username? [root] "
	read ans
	USER=${ans:-root}
    fi
    if [ x${OWNER} != x${USER} ]; then
	$ECHO "Currently checked out by ${OWNER}"
	exit 1
    fi
    rm ${CHECKEDOUT}
    ;;

update|up)
    update interactive
    cleanup
    ;;

create)
    # Since create does not modify the system, no need for checkedout
    ktcheck ${CASE} ${NETOPTS} -n -c sha1
    case "$?" in
    0)	;;

    1)	Yn "Update command file and/or transcripts?"
	if [ $? -eq 1 ]; then
	    ktcheck ${NETOPTS} -c sha1
	    RC=$?
	    if [ $RC -ne 1 ]; then
		$ECHO Nothing to update
		cleanup
		exit $RC
	    fi
	fi
	;;

    *)	cleanup
   	exit $?
    	;;
    esac
    $ECHO -n "Enter new transcript name [`hostname | cut -d. -f1`-`date +%Y%m%d`-${USER}.T]: "
    read TNAME
    if [ -z "${TNAME}" ]; then
	TNAME=`hostname | cut -d. -f1`-`date +%Y%m%d`-${USER}.T
    fi
    FTMP="${RASHTMP}/${TNAME}"
    fsdiff -C ${CASE} ${FPROGRESS} ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1;
    fi
    if [ ! -s ${FTMP} ]; then
	$ECHO Nothing to create.
	cleanup
	exit 1
    fi
    Yn "Edit transcript ${TNAME}?"
    if [ $? -eq 1 ]; then
	${EDITOR} ${FTMP}
    fi
    Yn "Store loadset ${TNAME}?"
    if [ $? -eq 1 ]; then
	if [ -n "${USERAUTH}" ]; then
	    if [ -z "${USERNAME}" ]; then
		$ECHO -n "username: "
		read USERNAME
	    fi
	    USERNAME="-U ${USERNAME}"
	fi
	lcreate ${PROGRESS} ${NETOPTS} ${USERAUTH} ${USERAUTH:+USERNAME} \
			${CHECKSUM} ${FTMP}
	if [ $? -ne 0 ]; then
	    cleanup
	    exit 1
	fi
    fi
    cleanup
    ;;

trip)
    if [ ! -f ${KFILE} ]; then
	$ECHO Command file missing, skipping tripwire.
	cleanup
	exit 1
    fi
    # Since trip does not modify the system, no need for checkedout
    ktcheck ${NETOPTS} -qn -c sha1
    case "$?" in
    0)
	;;
    1)
	$ECHO Command file and/or transcripts are out of date.
	;;
    *)
	cleanup
	exit $?
	;;
    esac

    fsdiff -C ${CASE} ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1
    fi
    if [ -s ${FTMP} ]; then
	$ECHO Trip failure: `hostname`
	cat ${FTMP}
	cleanup
	exit 0
    fi
    ;;

auto)
    checkedout
    if [ $? -eq 1 ]; then
	$ECHO "Checked out by ${OWNER}"
	exit 1
    fi
    fsdiff -C ${CASE} ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	$ECHO Auto failure: `hostname` fsdiff
	cleanup
	exit 1
    fi
    if [ -s ${FTMP} ]; then
	$ECHO Auto failure: `hostname` trip
	cat ${FTMP}
	cleanup
	exit 1
    fi

    # XXX - if this fails, do we loop, or just report error?
    ktcheck ${NETOPTS} -q -c sha1
    if [ $? -eq 1 ]; then
	while true; do
	    fsdiff -A ${CASE} ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
	    if [ $? -ne 0 ]; then
		$ECHO Auto failure: `hostname`: fsdiff
		cleanup
		exit 1
	    fi

	    cksum_generate
	    if ! cksum_compare; then
		$ECHO "Auto failure: `hostname`: difference cksum mismatch" 
		cksum_mismatch
		cleanup_and_exit
	    fi

	    dopreapply ${FTMP}
	    if [ -s ${FTMP} ]; then
		lapply ${NETOPTS} ${CASE} ${PROGRESS} \
			-q ${CHECKSUM} ${FTMP} 2>&1 > ${LTMP}
		case $? in
		0)
		    $ECHO Auto update: `hostname`
		    cat ${FTMP}
		    cksum_print
		    dopostapply ${FTMP}
		    cleanup
		    break
		    ;;

		*)
		    if [ ${RETRY} -gt 10000 ]; then
			$ECHO Auto failure: `hostname`
			cat ${LTMP}
			cleanup
			exit 1
		    fi
		    $ECHO Auto failure: `hostname` retrying
		    cat ${LTMP}
		    sleep ${RETRY}
		    RETRY=${RETRY}0
		    ktcheck ${NETOPTS} -q -c sha1
		    ;;
		esac
	    else
		$ECHO Nothing to apply.
		cleanup
		exit 0
	    fi
	done
    fi
    ;;

force)
    checkedout
    if [ $? -eq 1 ]; then
	$ECHO "Checked out by ${OWNER}"
	exit 1
    fi
    ktcheck ${NETOPTS} -c sha1
    case "$?" in
    0)	;;
    1)	;;

    *)	cleanup
    	exit $?
	;;
    esac

    fsdiff -A ${CASE} ${FPROGRESS} ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1
    fi

    if [ ! -s ${FTMP} ]; then
	$ECHO Nothing to apply.
	cleanup
	exit 0
    fi
    
    cksum_generate
    cksum_compare || cksum_mismatch
    dopreapply ${FTMP}
    lapply ${CASE} ${PROGRESS} ${NETOPTS} ${CHECKSUM} ${FTMP}
    case "$?" in
    0)	cksum_print
	;;

    *)	cleanup
	exit $?
	;;
    esac
    dopostapply ${FTMP}
    
    cleanup
    ;;

hook)
    update hook
    rc=$?
    while [ $rc -eq 1 ]; do
	update hook
	rc=$?
    done
    ;;

*)
    usage
    ;;

esac

cleanup
exit 0
