/*
 *	$Source: /afs/dev.mit.edu/source/repository/athena/bin/write/write.c,v $
 *	$Header: /afs/dev.mit.edu/source/repository/athena/bin/write/write.c,v 1.14 1995-11-30 19:48:15 miki Exp $
 */

#ifndef lint
static char *rcsid_write_c = "$Header: /afs/dev.mit.edu/source/repository/athena/bin/write/write.c,v 1.14 1995-11-30 19:48:15 miki Exp $";
#endif lint

#ifndef	lint
static char *sccsid = "@(#)write.c	4.13 3/13/86";
#endif
/*
 * write to another user
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <utmp.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pwd.h>

#define	NMAX	sizeof(ubuf.ut_name)
#define	LMAX	sizeof(ubuf.ut_line)

char	*strcat();
char	*strcpy();
struct	utmp ubuf;
int	signum[] = {SIGHUP, SIGINT, SIGQUIT, 0};
char	mebuf[NMAX + 1]	= "???";
char	*me;
char	*him;
int	netme = 0;
int	nethim = 0;
char	*mytty;
char	histty[32];
char	ttybuf[32];
char	*histtya;
char	myhost[32];
char	*hishost;
struct hostent *hp;
struct servent *sp;
struct sockaddr_in sin;
int	fds;
char	buf[128];
struct passwd *pwdent;
FILE	*tf;
int	logcnt;
char	*ttyname();
int	eof();
int	timout();
char	*getenv();

#ifdef _AIX
#define setpgrp setpgid
#endif

main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat stbuf;
	register i;
	register FILE *uf;
	int c1, c2;
	long clock = time(0);
	int suser = getuid() == 0;
	int nomesg = 0;
	struct tm *localtime();
	struct tm *localclock = localtime( &clock );

	me = mebuf;
	if ((argc > 3) && (getuid() == 0) &&
	    (strcmp("-f", argv[1]) == 0) &&
	    (mytty = strrchr(argv[2], '@'))) {
		me = argv[2];
		*mytty++ = '\0';
		netme = 1;
		argc -= 2;
		argv += 2;
	}
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: write user [ttyname]\n");
		exit(1);
	}
	him = argv[1];
	if ((!netme) && (hishost = strrchr(him, '@'))) {
		*hishost++ = '\0';
		hp = gethostbyname(hishost);
		if (hp == NULL) {
			static struct hostent def;
			static struct in_addr defaddr;
			static char namebuf[128];
			int inet_addr();

			defaddr.s_addr = inet_addr(hishost);
			if (defaddr.s_addr == -1) {
				printf("unknown host: %s\n", hishost);
				exit(1);
			}
			strcpy(namebuf, hishost);
			def.h_name = namebuf;
			def.h_addr = (char *)&defaddr;
			def.h_length = sizeof (struct in_addr);
			def.h_addrtype = AF_INET;
			def.h_aliases = 0;
			hp = &def;
		}
		nethim = 1;
	}

	if (argc == 3)
		histtya = argv[2];
	if ((uf = fopen("/etc/utmp", "r")) == NULL) {
		perror("write: Can't open /etc/utmp");
		if (histtya == 0)
			exit(10);
		goto cont;
	}
	if (!netme) {
	    mytty = ttyname(2);
	    if (mytty == NULL) {
		fprintf(stderr, "write: Can't find your tty\n");
		exit(1);
	    }
	    if (stat(mytty, &stbuf) < 0) {
		perror("write: Can't stat your tty");
		exit(1);
	    }
	    if ((stbuf.st_mode&020) == 0) {
		fprintf(stderr,
			"write: You have write permission turned off\n");
		if (!suser)
		  exit(1);
	    }
	    mytty = strrchr(mytty, '/') + 1;
	}
	if (histtya) {
		strcpy(histty, "/dev/");
		strcat(histty, histtya);
	}
	while (fread((char *)&ubuf, sizeof(ubuf), 1, uf) == 1) {
		if (ubuf.ut_name[0] == '\0')
			continue;
#if defined(_AIX) || defined(SYSV)
		if (ubuf.ut_type != USER_PROCESS)
			continue;
#endif
		if ((!netme) && strcmp(ubuf.ut_line, mytty)==0) {
			for (i=0; i<NMAX; i++) {
				c1 = ubuf.ut_name[i];
				if (c1 == ' ')
					c1 = 0;
				me[i] = c1;
				if (c1 == 0)
					break;
			}
		}
		if (nethim) goto nomat;
		if (him[0] == '-' && him[1] == 0)
			goto nomat;
		for (i=0; i<NMAX; i++) {
			c1 = him[i];
			c2 = ubuf.ut_name[i];
			if (c1 == 0)
				if (c2 == 0 || c2 == ' ')
					break;
			if (c1 != c2)
				goto nomat;
		}
		if (histtya && strncmp(histtya, ubuf.ut_line,
		    sizeof(ubuf.ut_line)))
			continue;
		logcnt++;
		if (histty[0]==0 || nomesg && histtya == 0) {
			strcpy(ttybuf, "/dev/");
			strcat(ttybuf, ubuf.ut_line);
			if (histty[0]==0)
				strcpy(histty, ttybuf);
			if (access(ttybuf, 0) < 0 || stat(ttybuf, &stbuf) < 0 ||
			    (stbuf.st_mode&020) == 0)
				nomesg++;
			else {
				strcpy(histty, ttybuf);
				nomesg = 0;
			}
		}
	nomat:
		;
	}
	fclose(uf);
	if (!nethim) { 
	    if (logcnt==0) {
		fprintf(stderr, "write: %s not logged in%s\n", him,
			histtya ? " on that tty" : "");
		exit(1);
	    }
	    if (histtya==0 && logcnt > 1) {
		fprintf(stderr,
			"write: %s logged in more than once ... writing to %s\n",
			him, histty+5);
	    }
#ifdef _AIX
	    /* This appears to flush the stderr buffer on the PS/2 so that
	       both sides of the connection don't receive the message. 
	       Wierd. - Ezra */
	    fflush(stderr);
#endif
	}
cont:
	fclose(uf);
	if((!netme) && (mebuf[0] == '?')) {
	    pwdent = getpwuid(getuid());
	    if (pwdent == NULL) {
		printf("You don't exist. Go away.\n");
		exit(-1);
	    }
	    strcpy(mebuf, pwdent->pw_name);
	}
	if (nethim) {
		sp = getservbyname("write", "tcp");
		if (sp == 0) {
			printf("tcp/write: unknown service\n");
			exit(1);
		}
		sin.sin_family = hp->h_addrtype;
                memmove((char *)&sin.sin_addr, hp->h_addr, hp->h_length);
		sin.sin_port = sp->s_port;
		fds = socket(hp->h_addrtype, SOCK_STREAM, 0);
		if (fds < 0) {
			perror("socket");
			exit(1);
		}
		if (connect(fds, (char *)&sin, sizeof (sin)) < 0) {
			perror("connect");
			close(fds);
			exit(1);
		}

		write(fds, me, strlen(me));
		write(fds, "@", 1);
		gethostname(myhost, sizeof (myhost));
		write(fds, myhost, strlen(myhost));
		write(fds, " ", 1);
		write(fds, him, strlen(him));
		if (histtya) {
			write(fds, " ", 1);
			write(fds, histtya, strlen(histtya));
		}
		write(fds, "\r\n", 2);
		sigs(eof);
		tf = fdopen(fds, "r+");
		while (1) {
			if (fgets(buf, sizeof(buf), tf) == NULL) exit(1);
			if (buf[0] == '\n') break;
			write(1, buf, strlen(buf));
		}
#ifdef SYSV
		rewind(tf); /* See the man page for fdopen(). write
			       won't work on the SGI without this. */
#endif
	} else {
	    if (access(histty, 0) < 0) {
		fprintf(stderr, "write: No such tty\n");
		exit(1);
	    }
	    signal(SIGALRM, timout);
	    alarm(5);
#if !defined(ultrix) && !defined(SYSV)
	    if (setpgrp(0,0))
	    if (setpgrp())
		 perror("setpgrp 0");
#endif
	    if (stat(histty, &stbuf) < 0 || (stbuf.st_mode&020) == 0
			|| (tf = fopen(histty, "w")) == NULL) {
		fprintf(stderr, "write: Permission denied\n");
		exit(1);
	    }
#ifndef SYSV
	    if (setpgrp(0,getpid()))
		 perror("setpgrp !0");
#endif
	    alarm(0);
	    sigs(eof);
	    if (netme) {
		printf("\n");
		fflush(stdout);
		fprintf(tf,
		 "\r\nMessage from %s on %s at %d:%02d ...\r\n\007\007\007",
		 me, mytty, localclock->tm_hour, localclock->tm_min);
	    } else
	    { char hostname[32];
	      gethostname(hostname, sizeof (hostname));
	      fprintf(tf,
		 "\r\nMessage from %s@%s on %s at %d:%02d ...\r\n\007\007\007",
		 me, hostname, mytty, localclock->tm_hour, localclock->tm_min);
	    }
	    fflush(tf);
	    fds = fileno(tf);
	}

	for (;;) {
		char buf[BUFSIZ];
		register char *bp;
		i = read(0, buf, sizeof buf);
		if (i <= 0)
			eof();
		if ((!netme) && buf[0] == '!') {
			buf[i] = 0;
			ex(buf);
			continue;
		}
		for (bp = buf; --i >= 0; bp++) {
			if (*bp == '\n')
				putc('\r', tf);

			if (!isascii(*bp)) {
				putc('M', tf);
				putc('-', tf);
				*bp = toascii(*bp);
			}

			if (isprint(*bp) ||
			    *bp == ' ' || *bp == '\t' || *bp == '\n'
			    || *bp == '\r') {
				putc(*bp, tf);
			} else {
				putc('^', tf);
				putc(*bp ^ 0100, tf);
			}

			if (*bp == '\n')
				fflush(tf);

			if (ferror(tf) || feof(tf)) {
				printf("\n\007Write failed (%s logged out?)\n",
					him);
				exit(1);
			}
		}
	}
}

timout()
{

	fprintf(stderr, "write: Timeout opening their tty\n");
	exit(1);
}

eof()
{

	if (!nethim) {
	    fprintf(tf, "EOF\r\n");
	    fflush(tf);
	}
	exit(0);
}

ex(bp)
	char *bp;
{
	register int i;

	sigs(SIG_IGN);
	i = fork();
	if (i < 0) {
		printf("Try again\n");
		goto out;
	}
	if (i == 0) {
		fclose(tf);		/* Close his terminal */
		setgid(getgid());	/* Give up effective group privs */
		sigs((int (*)())0);
		execl(getenv("SHELL") ?
		    getenv("SHELL") : "/bin/sh", "sh", "-c", bp+1, 0);
		exit(0);
	}
	while (wait((int *)NULL) != i)
		;
	printf("!\n");
out:
	sigs(eof);
}

sigs(sig)
	int (*sig)();
{
	register int i;

	for (i=0; signum[i]; i++)
#ifdef SYSV
		sigset(signum[i], sig);
#else
		signal(signum[i], sig);
#endif
}
