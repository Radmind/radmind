#!/bin/sh
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

SERVER="_RADMIND_HOST"
TLSLEVEL="_RADMIND_AUTHLEVEL"
EDITOR=${EDITOR:-vi}
DEFAULTS="/etc/defaults/radmind"
FSDIFFROOT="."
FLAG="_RADMIND_DIR/client/.RadmindRunning"
CHECKEDOUT="_RADMIND_DIR/client/.CheckedOut"
MAILDOMAIN="_RADMIND_MAIL_DOMAIN"
VERSION=_RADMIND_VERSION

PREAPPLY="_RADMIND_PREAPPLY"
POSTAPPLY="_RADMIND_POSTAPPLY"

PATH=/usr/local/bin:/usr/bin:/bin; export PATH
RETRY=10

MKTEMP="_RADMIND_MKTEMP"
TEMPFILES=FALSE
TMPDIR="/tmp/.ra.$$"
if [ -f "${MKTEMP}" ]; then
    TMPDIR=`${MKTEMP} -qd /tmp/.ra.$$.XXXXXX`
    if [ $? -ne 0 ]; then
	echo "mktemp failed"
	exit 1
    fi
fi
LTMP="${TMPDIR}/lapply.out"
FTMP="${TMPDIR}/fsdiff.out"

# different systems use different default dirs
if [ ! -f "${DEFAULTS}" ]; then
    DEFAULTS="/etc/default/radmind"
    if [ ! -f "${DEFAULTS}" ]; then
	DEFAULTS="/etc/radmind.defaults"
    fi
fi

Yn() {
    echo -n "$*" "[Yn] "
    read ans
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
    echo "Usage:	$0 [ -ctV ] [ -h server ] [ -w authlevel ] { trip | update | create | auto | force | checkout | checkin }" >&2
    exit 1
}

cleanup() {
    if [ "$TEMPFILES" = FALSE ]; then
	rm -fr "${TMPDIR}"
    fi
}

dopreapply() {
    if [ -d ${PREAPPLY} ]; then
	for script in ${PREAPPLY}/*; do
	    ${script} "$1"
	done
    fi
}

dopostapply() {
    if [ -d ${POSTAPPLY} ]; then
	for script in ${POSTAPPLY}/*; do
	    ${script} "$1"
	done
    fi
}

update() {
    opt="$1"
    kopt=

    checkedout
    if [ $? -eq 1 ]; then
	echo "Checked out by ${OWNER}"
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

    ktcheck ${kopt} -w ${TLSLEVEL} -h ${SERVER} -c sha1
    case "$?" in
    0)  if [ x"$opt" = x"hook" -a ! -f "${FLAG}" ]; then
	    cleanup
	    exit 0
	fi
	;;

    1)	if [ x"$opt" = x"interactive" ]; then
	    Yn "Update command file and/or transcripts?"
	    if [ $? -eq 1 ]; then
		ktcheck -w ${TLSLEVEL} -h ${SERVER} -c sha1
		RC=$?
		if [ $RC -ne 1 ]; then
		    echo Nothing to update
		    cleanup
		    exit $RC
		fi
	    fi
	fi
	;;

    *)	cleanup
    	exit $?
	;;
    esac

    fsdiff -A -% ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1
    fi

    if [ ! -s ${FTMP} ]; then
	echo Nothing to apply.
	cleanup
	exit 0
    fi
    if [ x"$opt" = x"interactive" ]; then
	Yn "Edit difference transcript?"
	if [ $? -eq 1 ]; then
	    ${EDITOR} ${FTMP}
	fi
    fi
    
    if [ x"$opt" = x"interactive" -a -d "${PREAPPLY}" ]; then
	Yn "Run pre-apply scripts on difference transcript?"
        if [ $? -eq 1 ]; then
            dopreapply ${FTMP}
        fi
    elif [ x"$opt" != x"interactive" ]; then
	dopreapply ${FTMP}
    fi
    if [ x"$opt" = x"interactive" ]; then
	Yn "Apply difference transcript?"
	if [ $? -ne 1 ]; then
	    cleanup
	    exit 0
	fi
    fi
    lapply ${PROGRESS} -w ${TLSLEVEL} -h ${SERVER} ${CHECKSUM} ${FTMP}
    case "$?" in
    0)	;;

    *)  if [ x"$opt" = x"hook" ]; then
	    echo -n "Applying changes failed, trying again "
	    echo "in ${RETRY} seconds..."
	    sleep ${RETRY}
	    RETRY=${RETRY}0

	    echo %OPENDRAWER
	    echo %BEGINPOLE
    	else 
	    cleanup
	fi
	return 1
	;;
    esac
    if [ x"$opt" = x"interactive" -a -d "${POSTAPPLY}" ]; then
	Yn "Run post-apply scripts on difference transcript?"
        if [ $? -eq 1 ]; then
            dopostapply ${FMTP}
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

while getopts %ch:ltVw: opt; do
    case $opt in
    %)  PROGRESS="-%"
	;;

    c)	CHECKSUM="-csha1"
	;;

    h)	SERVER="$OPTARG"
    	;;

    l)  USERAUTH="-l"
	;;

    t)	TEMPFILES="TRUE"
    	;;

    V)	echo ${VERSION}
	exit 0
	;;
	

    w)	TLSLEVEL="$OPTARG"
    	;;

    *)  usage
	;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 1 ]; then
	usage
fi

cd /

if [ ! -d ${TMPDIR} ]; then
    mkdir -m 700 ${TMPDIR} 
    if [ $? -ne 0 ]; then
        echo "Cannot create temporary directory $TMPDIR" 
	exit 1
    fi
fi

# Trap meaningful signals
trap cleanup HUP INT PIPE QUIT TERM TRAP XCPU XFSZ

case "$1" in
checkout)
    checkedout
    if [ $? -eq 1 ]; then
	if [ x${OWNER} = x${USER} ]; then
	    echo "Already checked out"
	    exit 1
	fi
	echo "Already checked out by ${OWNER}"
	Yn "Force checkout?"
	if [ $? -eq 0 ]; then
	    exit 1
	fi
	echo ${USER} has removed your checkout on `hostname` | mail -s `hostname`": Checkout broken" ${OWNER}@${MAILDOMAIN:-`hostname`}
    fi
    echo ${USER} > ${CHECKEDOUT}
    ;;

checkin)
    checkedout
    if [ $? -eq 0 ]; then
	echo "Not checked out"
	exit 1
    fi
    if [ x${OWNER} != x${USER} ]; then
	echo "Currently checked out by ${OWNER}"
	exit 1
    fi
    rm ${CHECKEDOUT}
    ;;

update)
    update interactive
    cleanup
    ;;

create)
    # Since create does not modify the system, no need for checkedout
    ktcheck -w ${TLSLEVEL} -h ${SERVER} -n -c sha1
    case "$?" in
    0)	;;

    1)	Yn "Update command file and/or transcripts?"
	if [ $? -eq 1 ]; then
	    ktcheck -w ${TLSLEVEL} -h ${SERVER} -c sha1
	    RC=$?
	    if [ $RC -ne 1 ]; then
		echo Nothing to update
		cleanup
		exit $RC
	    fi
	fi
	;;

    *)	cleanup
   	exit $?
    	;;
    esac
    echo -n "Enter new transcript name [`hostname | cut -d. -f1`-`date +%Y%m%d`-${USER}.T]: "
    read TNAME
    if [ -z "${TNAME}" ]; then
	TNAME=`hostname | cut -d. -f1`-`date +%Y%m%d`-${USER}.T
    fi
    FTMP="${TMPDIR}/${TNAME}"
    fsdiff -C -% ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1;
    fi
    if [ ! -s ${FTMP} ]; then
	echo Nothing to create.
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
	    echo -n "username: "
	    read USERNAME
	    USERNAME="-U ${USERNAME}"
	fi
	lcreate ${PROGRESS} -w ${TLSLEVEL} ${USERAUTH} ${USERNAME} \
			${CHECKSUM} -h ${SERVER} ${FTMP}
	if [ $? -ne 0 ]; then
	    cleanup
	    exit 1
	fi
    fi
    cleanup
    ;;

trip)
    # Since trip does not modify the system, no need for checkedout
    ktcheck -w ${TLSLEVEL} -h ${SERVER} -qn -c sha1
    case "$?" in
    0)
	;;
    1)
	echo Command file and/or transcripts are out of date.
	;;
    *)
	cleanup
	exit $?
	;;
    esac

    fsdiff -C ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1
    fi
    if [ -s ${FTMP} ]; then
	echo Trip failure: `hostname`
	cat ${FTMP}
	cleanup
	exit 0
    fi
    ;;

auto)
    checkedout
    if [ $? -eq 1 ]; then
	echo "Checked out by ${OWNER}"
	exit 1
    fi
    fsdiff -C ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	echo Auto failure: `hostname` fsdiff
	cleanup
	exit 1
    fi
    if [ -s ${FTMP} ]; then
	echo Auto failure: `hostname` trip
	cat ${FTMP}
	cleanup
	exit 1
    fi

    # XXX - if this fails, do we loop, or just report error?
    ktcheck -w ${TLSLEVEL} -h ${SERVER} -q -c sha1
    if [ $? -eq 1 ]; then
	while true; do
	    fsdiff -A ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
	    if [ $? -ne 0 ]; then
		echo Auto failure: `hostname`: fsdiff
		cleanup
		exit 1
	    fi
	    if [ -s ${FTMP} ]; then
		lapply -w ${TLSLEVEL} -h ${SERVER} -q ${CHECKSUM} \
			${FTMP} 2>&1 > ${LTMP}
		case $? in
		0)
		    echo Auto update: `hostname`
		    cat ${FTMP}
		    cleanup
		    break
		    ;;

		*)
		    if [ ${RETRY} -gt 10000 ]; then
			echo Auto failure: `hostname`
			cat ${LTMP}
			cleanup
			exit 1
		    fi
		    echo Auto failure: `hostname` retrying
		    cat ${LTMP}
		    sleep ${RETRY}
		    RETRY=${RETRY}0
		    ktcheck -w ${TLSLEVEL} -h ${SERVER} -q -c sha1
		    ;;
		esac
	    fi
	done
    fi
    ;;

force)
    checkedout
    if [ $? -eq 1 ]; then
	echo "Checked out by ${OWNER}"
	exit 1
    fi
    ktcheck -w ${TLSLEVEL} -h ${SERVER} -c sha1
    case "$?" in
    0)	;;
    1)	;;

    *)	cleanup
    	exit $?
	;;
    esac

    fsdiff -A -% ${CHECKSUM} -o ${FTMP} ${FSDIFFROOT}
    if [ $? -ne 0 ]; then
	cleanup
	exit 1
    fi

    if [ ! -s ${FTMP} ]; then
	echo Nothing to apply.
	cleanup
	exit 0
    fi
    
    dopreapply ${FTMP}
    lapply ${PROGRESS} -w ${TLSLEVEL} -h ${SERVER} ${CHECKSUM} ${FTMP}
    case "$?" in
    0)	;;

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
