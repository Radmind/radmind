# Do not use sym links when creating packages for OS X

DESTDIR=/usr/local
MANDIR=${DESTDIR}/share/man
BINDIR=${DESTDIR}/bin
SBINDIR=${DESTDIR}/sbin

# For server
VARDIR=/private/var/radmind
CONFIGFILE=${VARDIR}/config
TRANSCRIPTDIR=${VARDIR}/transcript

# For client
COMMANDFILE=${VARDIR}/client/command.K
GNU_DIFF=/usr/bin/diff
RADMIND_HOST=radmind

RADMINDSYSLOG=LOG_LOCAL7

# Solaris
#CC=	gcc
#CWARN=	-Wall -Wmissing-prototypes -Wconversion
#ADDLIBS=	-lnsl -lsocket
#INSTALL=	/usr/ucb/install
#OPENSSL=	/usr/local/openssl

# MacOSX
CC=	cc
CWARN=	-Wall -Wmissing-prototypes -Wconversion
ADDLIBS=
INSTALL=	install
OPENSSL=

#
# Should not need to edit anything after here.
#

CFLAGS=		${CWARN} ${OSNAME} ${INCPATH}
INCPATH=	-I${OPENSSL}/include -Ilibsnet
LDFLAGS=	-L${OPENSSL}/lib -Llibsnet ${ADDLIBS} -lsnet -lcrypto

BINTARGETS=     fsdiff ktcheck lapply lcksum lcreate lmerge lfdiff twhich
MAN1TARGETS=    fsdiff.1 ktcheck.1 lapply.1 lcksum.1 lcreate.1 lfdiff.1 \
                lmerge.1 twhich.1
TARGETS=        radmind ${BINTARGETS}

RADMIND_OBJ=    version.o daemon.o command.o argcargv.o code.o \
                cksum.o base64.o mkdirs.o applefile.o connect.o

FSDIFF_OBJ=     version.o fsdiff.o argcargv.o transcript.o llist.o code.o \
                hardlink.o cksum.o base64.o pathcmp.o radstat.o applefile.o \
                connect.o

KTCHECK_OBJ=    version.o ktcheck.o argcargv.o retr.o base64.o code.o \
                cksum.o list.o connect.o applefile.o

LAPPLY_OBJ=     version.o lapply.o argcargv.o code.o base64.o retr.o \
                radstat.o update.o cksum.o connect.o pathcmp.o \
                applefile.o

LCREATE_OBJ=    version.o lcreate.o argcargv.o code.o connect.o \
                stor.o applefile.o base64.o cksum.o radstat.o

LCKSUM_OBJ=     version.o lcksum.o argcargv.o cksum.o base64.o code.o \
                pathcmp.o applefile.o connect.o

LMERGE_OBJ=     version.o lmerge.o argcargv.o code.o pathcmp.o mkdirs.o

LFDIFF_OBJ=     version.o lfdiff.o argcargv.o connect.o retr.o cksum.o \
                base64.o applefile.o code.o

TWHICH_OBJ=     version.o argcargv.o code.o twhich.o pathcmp.o

all : ${TARGETS}

version.o : version.c
	${CC} ${CFLAGS} \
		-DVERSION=\"`cat VERSION`\" \
		-c version.c

daemon.o : daemon.c
	${CC} ${CFLAGS} \
		-D_PATH_RADMIND=\"${VARDIR}\" -DLOG_RADMIND=${RADMINDSYSLOG} \
		-c daemon.c

command.o : command.c
	${CC} ${CFLAGS} \
		-D_PATH_CONFIG=\"${CONFIGFILE}\" \
		-D_PATH_TRANSCRIPTS=\"${TRANSCRIPTDIR}\" \
		-c command.c

fsdiff.o : fsdiff.c
	${CC} ${CFLAGS} \
		-D_RADMIND_COMMANDFILE=\"${COMMANDFILE}\" \
		-c fsdiff.c

lfdiff.o : lfdiff.c
	${CC} ${CFLAGS} \
		-D_PATH_GNU_DIFF=\"${GNU_DIFF}\" \
		-D_RADMIND_HOST=\"${RADMIND_HOST}\" \
		-c lfdiff.c

ktcheck.o : ktcheck.c
	${CC} ${CFLAGS} \
		-D_RADMIND_HOST=\"${RADMIND_HOST}\" \
		-D_RADMIND_COMMANDFILE=\"${COMMANDFILE}\" \
		-c ktcheck.c

lapply.o : lapply.c
	${CC} ${CFLAGS} \
		-D_RADMIND_HOST=\"${RADMIND_HOST}\" \
		-c lapply.c

lcreate.o : lcreate.c
	${CC} ${CFLAGS} \
		-D_RADMIND_HOST=\"${RADMIND_HOST}\" \
		-c lcreate.c

twhich.o : twhich.c
	${CC} ${CFLAGS} \
		-D_RADMIND_COMMANDFILE=\"${COMMANDFILE}\" \
		-c twhich.c

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

lfdiff: ${LFDIFF_OBJ}
	${CC} -o lfdiff ${LFDIFF_OBJ} ${LDFLAGS}

twhich: ${TWHICH_OBJ}
	${CC} -o twhich ${TWHICH_OBJ} ${LDFLAGS}


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

install	: all
	-mkdir -p ${DESTDIR}
	-mkdir -p ${SBINDIR}
	${INSTALL} -m 0755 -c radmind ${SBINDIR}/
	-mkdir -p ${BINDIR}
	for i in ${BINTARGETS}; do \
	    ${INSTALL} -m 0755 -c $$i ${BINDIR}/; \
	done
	-mkdir -p ${MANDIR}
	-mkdir ${MANDIR}/man1
	for i in ${MAN1TARGETS}; do \
	    ${INSTALL} -m 0644 -c $$i ${MANDIR}/man1/; \
	done

CLIENTBINPKGDIR=${DISTDIR}/client-bin
CLIENTMANPKGDIR=${DISTDIR}/client-man
CLIENTVARPKGDIR=${DISTDIR}/client-var
SERVERSTARTUPPKGDIR=${DISTDIR}/server-startup
SERVERSBINPKGDIR=${DISTDIR}/server-sbin

package : all
	# Create server package #
	mkdir -p -m 0755 ${SERVERSBINPKGDIR}
	${INSTALL} -o root -g wheel -m 0555 -c radmind ${SERVERSBINPKGDIR}
	mkdir -m 0755 ${SERVERSTARTUPPKGDIR} 
	${INSTALL} -o root -g wheel -m 0755 -c OS_X/RadmindServer \
	    ${SERVERSTARTUPPKGDIR}
	${INSTALL} -o root -g wheel -m 0644 -c OS_X/StartupParameters.plist \
	    ${SERVERSTARTUPPKGDIR}

	# Create client package #
	mkdir -p -m 0755 ${CLIENTBINPKGDIR}
	for i in ${BINTARGETS}; do \
	    ${INSTALL} -o root -g wheel -m 0555 -c $$i \
		${CLIENTBINPKGDIR}/; \
	done
	mkdir -p -m 0755 ${CLIENTMANPKGDIR}/man1
	for i in ${MAN1TARGETS}; do \
	    ${INSTALL} -o root -g wheel -m 0444 -c $$i \
		${CLIENTMANPKGDIR}/man1/; \
	done 
	mkdir -p -m 0755 ${CLIENTVARPKGDIR}
	${INSTALL} -o root -g staff -m 0755 -c OS_X/command.K \
	    ${CLIENTVARPKGDIR}
	chown root:wheel ${DISTDIR}
	find ${DISTDIR}/* -exec chown root:wheel {} \;
	package ${CLIENTBINPKGDIR} OS_X/client-bin.info -d ${DISTDIR}
	package ${CLIENTMANPKGDIR} OS_X/client-man.info -d ${DISTDIR}
	package ${CLIENTVARPKGDIR} OS_X/client-var.info -d ${DISTDIR}
	package ${SERVERSTARTUPPKGDIR} OS_X/server-startup.info -d ${DISTDIR}
	package ${SERVERSBINPKGDIR} OS_X/server-sbin.info -d ${DISTDIR}
	rm -rf ${CLIENTBINPKGDIR} ${CLIENTMANPKGDIR} ${CLIENTVARPKGDIR} ${SERVERSTARTUPPKGDIR} ${SERVERSBINPKGDIR}
	cp OS_X/radmind.info ${DISTDIR}
	cp OS_X/radmind.list ${DISTDIR}
	mv ${DISTDIR} ../radmind.mpkg
	#cd ..; tar zvcf radmind.mpkg.tgz radmind.mpkg/*

clean :
	rm -f *.o a.out core
	rm -f ${TARGETS}
