#!/bin/sh
#
# Radmind Assistant shell script (rash)
# Basic client side functions include:
#
#	trip
#	update
#	create
#	auto
#

# Command line options:
# -c  without checksums

# Config file stuff:
#
# server
# -w # -x, -y, -z
# lcreate -l -U
# -c sha1

PATH=/usr/bin:/bin; export PATH
RETRY=10

Yn() {
    echo -n "$*" "[Yn] "
    read ans
    if [ -z "$ans" -o X"$ans" = Xy -o X"$ans" = XY -o X"$ans" = Xyes ]; then
	return 1
    fi
    return 0
}

usage() {
    echo "Usage:	$0 [-c] { trip | update | create | auto }" >&2
    exit 1
}

while getopts c opt; do
    case $opt in
    c)	CHECKSUM="-csha1"
	;;

    *)   usage
	;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 1 ]; then
	usage;
fi

cd /

case "$1" in
update)
    ktcheck -n -c sha1
    if [ $? -eq 1 ]; then
	Yn "Update command file and/or transcripts?"
	if [ $? -eq 1 ]; then
	    ktcheck -c sha1
	fi
    fi
    fsdiff -A -v ${CHECKSUM} -o /tmp/ra.fsdiff.$$ .
    if [ ! -s /tmp/ra.fsdiff.$$ ]; then
	echo Nothing to apply.
	rm /tmp/ra.fsdiff.$$
	exit 1
    fi
    Yn "Edit difference transcript?"
    if [ $? -eq 1 ]; then
	${EDITOR} /tmp/ra.fsdiff.$$
    fi
    Yn "Apply difference transcript?"
    if [ $? -eq 1 ]; then
	lapply ${CHECKSUM} /tmp/ra.fsdiff.$$
    fi
    rm /tmp/ra.fsdiff.$$
    ;;

create)
    ktcheck -n -c sha1
    if [ $? -eq 1 ]; then
	Yn "Update command file and/or transcripts?"
	if [ $? -eq 1 ]; then
	    ktcheck -c sha1
	fi
    fi
    fsdiff -C -v ${CHECKSUM} -o /tmp/ra.fsdiff.$$ .
    if [ ! -s /tmp/ra.fsdiff.$$ ]; then
	echo Nothing to create.
	rm /tmp/ra.fsdiff.$$
	exit 1
    fi
    Yn "Edit difference transcript?"
    if [ $? -eq 1 ]; then
	${EDITOR} /tmp/ra.fsdiff.$$
    fi
    Yn "Store difference transcript?"
    if [ $? -eq 1 ]; then
	lcreate ${CHECKSUM} /tmp/ra.fsdiff.$$
    fi
    rm /tmp/ra.fsdiff.$$
    ;;

trip)
    ktcheck -qn -c sha1
    case "$?" in
    0)
	;;
    1)
	echo Command file and/or transcripts are out of date.
	;;
    *)
	echo Command file and/or transcripts unknown.
	;;
    esac

    fsdiff -C ${CHECKSUM} -o /tmp/ra.fsdiff.$$ .
    # Check return value?
    if [ -s /tmp/ra.fsdiff.$$ ]; then
	echo Trip failure: `hostname`
	cat /tmp/ra.fsdiff.$$
	exit 0
    fi
    ;;

auto)
    fsdiff -C ${CHECKSUM} -o /tmp/ra.fsdiff.$$ .
    if [ -s /tmp/ra.fsdiff.$$ ]; then
	echo Trip failure: `hostname`
	cat /tmp/ra.fsdiff.$$
	exit 0
    fi

    ktcheck -q -c sha1
    if [ $? -eq 1 ]; then
	while true; do
	    fsdiff -A ${CHECKSUM} -o /tmp/ra.fsdiff.$$
	    if [ $? -ne 0 ]; then
		echo fsdiff failed
		exit 1
	    fi
	    if [ -s /tmp/ra.fsdiff.$$ ]; then
		lapply -q ${CHECKSUM} /tmp/ra.fsdiff.$$ 2>&1 > /tmp/ra.lapply.$$
		case $? in
		0)
		    echo Auto update: `hostname`
		    cat /tmp/ra.fsdiff.$$
		    rm /tmp/ra.fsdiff.$$
		    rm /tmp/ra.lapply.$$
		    break
		    ;;

		*)
		    if [ ${RETRY} -gt 10000 ]; then
			echo Auto failure: `hostname`
			cat /tmp/ra.lapply.$$
			exit 1
		    fi
		    echo Auto failure: `hostname` retrying
		    cat /tmp/ra.lapply.$$
		    sleep ${RETRY}
		    RETRY=${RETRY}0
		    ktcheck -q -c sha1
		    ;;
		esac
	    fi
	done
    fi
    ;;

*)
    usage
    ;;

esac

exit 0
