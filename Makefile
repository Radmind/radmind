VARDIR=/var/radmind
CONFIGFILE=${VARDIR}/config
TRANSCRIPTDIR=${VARDIR}/transcript

RADMINDSYSLOG=LOG_LOCAL7

# Stock compiler
#CC=cc

# For gcc
CC=	gcc
CWARN=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wconversion -Werror
OSNAME= -DSOLARIS
CFLAGS= ${CWARN} ${OSNAME} ${INCPATH}

OPENSSL=	/usr/local/openssl

INCPATH=  -I${OPENSSL}/include/openssl -Ilibsnet
LDFLAGS=  -L${OPENSSL}/lib -Llibsnet -lnsl -lsnet -lcrypto -lsocket

TARGETS=	radmind fsdiff ktcheck lapply lcksum lcreate lmerge

RADMIND_OBJ=	version.o daemon.o command.o argcargv.o auth.o code.o \
		chksum.o base64.o

FSDIFF_OBJ=	version.o fsdiff.o argcargv.o transcript.o llist.o code.o \
		hardlink.o chksum.o base64.o

KTCHECK_OBJ=	version.o ktcheck.o argcargv.o download.o base64.o code.o \
		chksum.o list.o connect.o

LAPPLY_OBJ=	version.o lapply.o argcargv.o code.o base64.o download.o \
		convert.o update.o chksum.o copy.o connect.o

LCREATE_OBJ=	version.o lcreate.o argcargv.o code.o connect.o

LCKSUM_OBJ=	version.o lcksum.o argcargv.o chksum.o base64.o code.o

LMERGE_OBJ=	version.o lmerge.o argcargv.o pathcmp.o mkdirs.o

all : ${TARGETS}

version.o : version.c
	${CC} ${CFLAGS} -DVERSION=\"`cat VERSION`\" -c version.c

daemon.o : daemon.c
	${CC} ${CFLAGS} \
		-D_PATH_RADMIND=\"${VARDIR}\" -DLOG_RADMIND=${RADMINDSYSLOG} \
		-c daemon.c

command.o : command.c
	${CC} ${CFLAGS} \
		-D_PATH_CONFIG=\"${CONFIGFILE}\" \
		-D_PATH_TRANSCRIPTS=\"${TRANSCRIPTDIR}\" \
		-c command.c

radmind : libsnet/libsnet.a ${RADMIND_OBJ} Makefile
	${CC} ${CFLAGS} -o radmind ${RADMIND_OBJ} ${LDFLAGS}

fsdiff : ${FSDIFF_OBJ}
	${CC} -o fsdiff ${FSDIFF_OBJ} ${LDFLAGS}

ktcheck: ${KTCHECK_OBJ}
	${CC} -o ktcheck ${KTCHECK_OBJ} ${LDFLAGS}

lapply: ${LAPPLY_OBJ}
	${CC} -o lapply ${LAPPLY_OBJ} ${LDFLAGS}

lcksum: ${LCKSUM_OBJ}
	${CC} -o lcksum ${LCKSUM_OBJ} ${LDFLAGS}

lcreate: ${LCREATE_OBJ}
	${CC} -o lcreate ${LCREATE_OBJ} ${LDFLAGS}

lmerge: ${LMERGE_OBJ}
	${CC} -o lmerge ${LMERGE_OBJ} ${LDFLAGS}

FRC :

libsnet/libsnet.a : FRC
	cd libsnet; ${MAKE} ${MFLAGS} CC=${CC}

VERSION=`date +%Y%m%d`
DISTDIR=../radmind-${VERSION}

dist   : clean
	mkdir ${DISTDIR}
	tar chfFFX - EXCLUDE . | ( cd ${DISTDIR}; tar xvf - )
	chmod +w ${DISTDIR}/Makefile
	echo ${VERSION} > ${DISTDIR}/VERSION

clean :
	rm -f *.o a.out core
	rm -f ${TARGETS}
