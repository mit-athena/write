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
static char *rcsid_writed_c = "$Header: /afs/dev.mit.edu/source/repository/athena/bin/write/writed.c,v 1.1 1985-12-06 23:31:08 wesommer Exp $";
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
	char line[512];
	struct sockaddr_in sin;
	int i, p[2], pid, status;
	FILE *fp;
	char *av[10];

	i = sizeof (sin);
	if (getpeername(0, &sin, &i) < 0)
		fatal(argv[0], "getpeername");
	line[0] = '\0';

	gets(line);
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
		while (*sp && !isspace(*sp)) sp++;
		if (*sp) *sp++ = '\0';
	}
	av[i] = 0;
	if (pipe(p) < 0)
		fatal(argv[0], "pipe");
	if ((pid = fork()) == 0) {
		close(p[0]);
		if (p[1] != 1) {
			dup2(p[1], 1);
			close(p[1]);
		}
		execv("/bin/write", av);
		_exit(1);
	}
	if (pid == -1)
		fatal(argv[0], "fork");
	close(p[1]);
	if ((fp = fdopen(p[0], "r")) == NULL)
		fatal(argv[0], "fdopen");
	while ((i = getc(fp)) != EOF) {
		if (i == '\n')
			putchar('\r');
		putchar(i);
	}
	fclose(fp);
	while ((i = wait(&status)) != pid && i != -1)
		;
	return(0);
}

fatal(prog, s)
	char *prog, *s;
{

	fprintf(stderr, "%s: ", prog);
	perror(s);
	exit(1);
}
