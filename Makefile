#
#	Makefile for write
#
DESTDIR=
CFLAGS= -O

PROGRAM=	write
SRCS=		write.c
LIBS=		

${PROGRAM}:	${SRCS}
	${CC} ${CFLAGS} -o $@ $@.c ${LIBS}

install:
	install -g tty -m 2755 -s ${PROGRAM} ${DESTDIR}/bin/${PROGRAM}

tags:
	ctags -tdw *.c 

clean:
	rm -f ${PROGRAM} a.out core *.s *.o made


depend: 
	touch Make.depend; makedepend -fMake.depend -o "" -s "# DO NOT DELETE THIS LINE -- make depend uses it"\
		${CFLAGS} ${SRCS}

