#!/bin/sh
#
# Certificate Authority shell script (cash)
# Basic client side functions include:
#
#	trip
#	update
#	create
#	auto
#	force
#

EDITOR=${EDITOR:-vi}
MAILDOMAIN="_RADMIND_MAIL_DOMAIN"
VERSION=_RADMIND_VERSION
RADMINDDIR=/var/radmind
#CADIR=${RADMINDDIR}/ca
CERTDIR=${RADMINDDIR}/cert
SPECIALDIR=${RADMINDDIR}/special
CLIENTCERTDIR=/private/var/radmind/cert
CADIR=./CA
OPENSSLCNF=openssl2.cnf

PATH=/usr/local/bin:/usr/bin:/bin; export PATH

MKTEMP="_RADMIND_MKTEMP"
TEMPFILES=FALSE
TMPDIR="/tmp/.ca.$$"
if [ -f "${MKTEMP}" ]; then
    TMPDIR=`${MKTEMP} -qd /tmp/.ca.$$.XXXXXX`
    if [ $? -ne 0 ]; then
	echo "mktemp failed"
	exit 1
    fi
fi

makecert() {
    if [ -z $1 ]; then
	echo -n "Enter new certificate common name: "
	read CN
	if [ -z ${CN} ]; then
	    echo "Invalid common name"
	    cleanup
	    exit 1
	fi
    else
	CN=$1
    fi
    CERT=`grep /CN=${CN}/ index.txt | grep ^V | cut -f4`
    if [ ! -z ${CERT} ]; then
	echo Using existing certificate ${CERT}.pem
	return 0
    fi
    openssl req -new -keyout keys/${CN}.key -out csrs/${CN}.csr -days 360 -config openssl2.cnf -nodes <<EOF





${CN}
mcneal@umich.edu
EOF
    openssl ca -in csrs/${CN}.csr -out ${TMPDIR}/${CN}.pem -batch -config openssl2.cnf
    CERT=`grep /CN=${CN}/ index.txt | grep ^V | cut -f4`
    if [ -z ${CERT} ]; then
	echo Certificate creation failed
	cleanup
	exit 1
    fi
    return 0
}

Yn() {
    echo -n "$*" "[Yn] "
    read ans
    if [ -z "$ans" -o X"$ans" = Xy -o X"$ans" = XY -o X"$ans" = Xyes ]; then
	return 1
    fi
    return 0
}

usage() {
    echo "Usage:	$0 [ -V ] { init | create }" >&2
    exit 1
}

cleanup() {
    if [ "$TEMPFILES" = FALSE ]; then
	rm -fr "${TMPDIR}"
    fi
}

while getopts V opt; do
    case $opt in
    V)	echo ${VERSION}
	exit 0
	;;
	
    *)  usage
	;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 1 ]; then
	usage
fi

if [ ! -d ${TMPDIR} ]; then
    mkdir -m 700 ${TMPDIR} 
    if [ $? -ne 0 ]; then
        echo "Cannot create temporary directory $TMPDIR" 
	exit 1
    fi
fi

# Trap meaningful signals
trap cleanup HUP INT PIPE QUIT TERM TRAP XCPU XFSZ
echo $1

case "$1" in
init)
    # Setup Certificate Authority
    if [ -d ${CADIR} ]; then
	echo ${CADIR} already exists
	Yn "Remove existing certificate authority?"
	if [ $? -eq 0 ]; then
	    cleanup
	    exit 1
	fi
	rm -rf ${CADIR}
	if [ $? -ne 0 ]; then
	    echo "Cannot remove old certificate authority"
	    cleanup
	    exit 1
	fi
    fi

    mkdir -p ${CADIR}
    if [ $? -ne 0 ]; then
	echo "Cannot create ${CADIR}"
	cleanup
	exit 1
    fi
    cd ${CADIR}
    if [ $? -ne 0 ]; then
	echo "Cannot cd to ${CADIR}"
	cleanup
	exit 1
    fi
    mkdir certs private keys csrs
    if [ $? -ne 0 ]; then
	echo "Cannot create directories"
	cleanup
	exit 1
    fi
    echo "01" > serial
    touch index.txt
    wget http://radmind.org/files/openssl2.cnf
    if [ $? -ne 0 ]; then
	echo "Cannot get default ${OPENSSLCNF}"
	cleanup
	exit 1
    fi
    openssl req -config ${OPENSSLCNF} -x509 -newkey rsa -days 1825 -keyout private/ca-key.pem -out certs/ca-cert.pem -outform PEM
    makecert `hostname`

    ;;

create)
    if [ ! -d ${CADIR} ]; then
        echo "No certificate authority"
        cleanup
        exit 1
    fi
    cd ${CADIR}

    makecert

    cat certs/${CERT}.pem keys/${CN}.key > ${CERTDIR}/${CN}.pem
    if [ $? -ne 0 ]; then
	echo Unable to create ${CERTDIR}/${CN}.pem
	cleanup
	exit 1
    fi
    if [ ! -d ${SPECIALDIR}/${CN}/${CLIENTCERTDIR} ]; then
	mkdir -p ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}
	if [ $? -ne 0 ]; then
	    echo Unable to create ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}
	    cleanup
	    exit 1
	fi
    fi
    if [ -a ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}/cert.pem ]; then
	rm -f ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}/cert.pem
	echo Unable to remove old ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}/cert.pem
	cleanup
	exit 1
    fi

    ln -s ${CERTDIR}/${CN}.pem ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}/cert.pem
    if [ $? -ne 0 ]; then
	echo Unable to link ${CERTDIR}/${CN}.pem to ${SPECIALDIR}/${CN}/${CLIENTCERTDIR}/cert.pem
	cleanup
	exit 1
    fi
    ;;

*)
    usage
    ;;

esac

cleanup
exit 0
