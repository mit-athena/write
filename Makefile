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
	makedepend -o "" -s "# DO NOT DELETE THIS LINE -- make depend uses it"\
		${CFLAGS} ${SRCS}

# DO NOT DELETE THIS LINE -- make depend uses it

write: /usr/include/stdio.h /usr/include/ctype.h /usr/include/sys/types.h
write: /usr/include/sys/stat.h /usr/include/signal.h /usr/include/utmp.h
write: /usr/include/sys/time.h /usr/include/sys/time.h
write: /usr/include/sys/socket.h /usr/include/netinet/in.h
write: /usr/include/netdb.h /usr/include/pwd.h
