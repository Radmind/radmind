# Stock compiler
#CC=cc

# For gcc
CC=	gcc
CWARN=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wconversion -Werror
OSNAME= -DSOLARIS
CFLAGS= ${CWARN} ${OSNAME} ${INCPATH}
INCPATH=  -I/usr/local/openssl/include/openssl -Ilibsnet
LDFLAGS=  -L/usr/local/openssl/lib -Llibsnet -lnsl -lsnet -lcrypto -lsocket

VERSION=`date +%Y%m%d`

FSDIFF_OBJ =	fsdiff.o argcargv.o transcript.o llist.o code.o hardlink.o \
		chksum.o base64.o

KTCHECK_OBJ =	ktcheck.o argcargv.o download.o base64.o code.o chksum.o \
		list.o connect.o

LAPPLY_OBJ =	lapply.o argcargv.o code.o base64.o download.o convert.o \
		update.o chksum.o copy.o connect.o

LCREATE_OBJ =	lcreate.o argcargv.o code.o connect.o

LCKSUM_OBJ =	lcksum.o argcargv.o chksum.o base64.o code.o

LMERGE_OBJ =	lmerge.o argcargv.o pathcmp.o mkdirs.o

all : fsdiff ktcheck lapply lcreate lcksum lmerge

fsdiff : ${FSDIFF_OBJ}
	${CC} -o fsdiff ${FSDIFF_OBJ} ${LDFLAGS}

fsdiff.o : fsdiff.c
	${CC} ${CFLAGS} -DVERSION=\"`cat VERSION`\" -c fsdiff.c

ktcheck: ${KTCHECK_OBJ}
	${CC} -o ktcheck ${KTCHECK_OBJ} ${LDFLAGS}

lapply: ${LAPPLY_OBJ}
	${CC} -o lapply ${LAPPLY_OBJ} ${LDFLAGS}

lcksum: ${LCKSUM_OBJ}
	${CC} -o lcksum ${LCKSUM_OBJ} ${LDFLAGS}

lcreate: ${LCREATE_OBJ}
	${CC} -o lcreate ${LCREATE_OBJ} ${LDFLAGS}

lcreate.o : lcreate.c
	${CC} ${CFLAGS} -DVERSION=\"`cat VERSION`\" -c lcreate.c

lmerge: ${LMERGE_OBJ}
	${CC} -o lmerge ${LMERGE_OBJ} ${LDFLAGS}

clean :
	rm -f *.o a.out core
	rm -f fsdiff ktcheck lapply lcksum lcreate lmerge
