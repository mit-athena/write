/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
/*
 * Copyright (c) 1985 Massachusetts Institute of Technology
 * Note:  This is mostly fingerd.c with all occurances of
 * "finger" changed to "write".
 */
#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
static char *rcsid_writed_c = "$Header: /afs/dev.mit.edu/source/repository/athena/bin/write/writed.c,v 1.4 1991-08-02 14:15:05 epeisach Exp $";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)writed.c	5.1 (Berkeley) 6/6/85";
#endif not lint

/*
 * Write server.
 */
#include <sys/types.h>
#include <netinet/in.h>

#include <stdio.h>
#include <ctype.h>

main(argc, argv)
	char *argv[];
{
	register char *sp;
	char line[BUFSIZ];
	struct sockaddr_in sin;
	int i;
	char *av[10]; /* space for 9 arguments, plus terminating null */


	i = sizeof (sin);
	if (getpeername(0, &sin, &i) < 0)
		fatal(argv[0], "getpeername");
	line[0] = '\0';

	fgets(line, BUFSIZ, stdin);
	sp = line;
	av[0] = "write";
	av[1] = "-f";
	i = 2;
	while (1) {
		while (isspace(*sp))
			sp++;
		if (!*sp)
			break;
		av[i++] = sp;
		if (i == 9)
			/* past end of av space -- throw out the rest */
			/* of the args				      */
			break;
		while (*sp && !isspace(*sp)) sp++;
		if (*sp) *sp++ = '\0';
	}
	av[i] = 0;
	/* Put the socket on stdin, stdout, and stderr */
	dup2(0, 1);
	dup2(0, 2);
#if (defined(vax) && !defined(ultrix) || defined(ibm032))
	execv("/bin/write", av);
#else
	execv("/usr/athena/bin/write", av);
#endif
	_exit(1);
}

fatal(prog, s)
	char *prog, *s;
{

	fprintf(stderr, "%s: ", prog);
	perror(s);
	exit(1);
}
