/* Copyright 1986-1999 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

/* This is the client side of the networked write system. */

static const char rcsid[] = "$Id: main.c,v 1.2 1999-10-30 19:05:05 ghudson Exp $";

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_GETUTXENT
#include <utmpx.h>
#else /* HAVE_GETUTXENT */
#include <utmp.h>
#ifndef UTMP_FILE
#ifdef _PATH_UTMP
#define UTMP_FILE _PATH_UTMP
#else
#define UTMP_FILE "/var/adm/utmp"
#endif
#endif /* UTMP_FILE */
struct utmp *getutxent(void);
struct utmp *getutxline(const struct utmp *line);
void setutxent(void);
#endif /* HAVE_GETUTXENT */

#ifndef HAVE_INET_ATON
static int inet_aton(const char *str, struct in_addr *addr);
#endif

static int oktty(char *tty);
static void eof(int);
static int read_line(FILE *fp, char **buf, int *bufsize);

int fromnet, tonet;
FILE *f;

int main(int argc, char **argv)
{
  int status, suser = getuid() == 0;
  char *me, *myhost = NULL, *mytty = NULL;
  char *him, *hishost = NULL, *histty = NULL, *histtyname = NULL;
  char *p;
  struct hostent *h;
#ifdef HAVE_GETUTXENT
  struct utmpx *ut, line;
#else
  struct utmp *ut, line;
#endif
  char *buf = NULL;
  int fd, bufsize;
  struct sigaction sa;

  /* Determine the writing user. If invoked from writed, this will
   * be passed as an argument. Otherwise, check utmp or passwd.
   */
  if (argc > 3 && suser && !strcmp(argv[1], "-f"))
    {
      struct sockaddr_in sin;
      int sinlen = sizeof(sin);

      fromnet = 1;
      me = argv[2];

      /* The host should be passed in as part of the argument, but
       * we'll ignore what it says and figure it out for ourselves.
       */
      p = strchr(me, '@');
      if (p)
	*p = '\0';

      if (getpeername(0, (struct sockaddr *)&sin, &sinlen) != 0)
	{
	  perror("write: getpeername failed");
	  exit(1);
	}
      h = gethostbyaddr((char *)&sin.sin_addr, sinlen, AF_INET);
      if (!h)
	{
	  fprintf(stderr, "write: Can't determine your hostname.\n");
	  exit(1);
	}
      myhost = strdup(h->h_name);

      argv += 2;
      argc -= 2;
    }
  else
    {
      char hostbuf[256];

      mytty = ttyname(0);
      if (!mytty)
	mytty = ttyname(1);
      if (!mytty)
	mytty = ttyname(2);
      if (!mytty)
	{
	  fprintf(stderr, "write: Can't find your tty.\n");
	  exit(1);
	}

      /* Check for writability. */
      if (!oktty(mytty))
	{
	  fprintf(stderr, "write: You have write permission turned off.\n");
	  if (!suser)
	    exit(1);
	}

      p = strchr(mytty + 1, '/');
      if (p)
	mytty = p + 1;

      strncpy(line.ut_line, mytty, sizeof(line.ut_line));
      ut = getutxline(&line);
      if (ut)
	{
	  me = malloc(sizeof(ut->ut_name) + 1);
	  strncpy(me, ut->ut_name, sizeof(ut->ut_name));
	  me[sizeof(ut->ut_name)] = '\0';
	}
     else
	{
	  struct passwd *pw = getpwuid(getuid());
	  if (!pw)
	    {
	      fprintf(stderr, "write: Can't figure out who you are.\n");
	      exit(1);
	    }
	  me = strdup(pw->pw_name);
	}

      h = NULL;
      if (!gethostname(hostbuf, sizeof(hostbuf)))
	h = gethostbyname(hostbuf);
      if (!h)
	{
	  fprintf(stderr, "write: Can't determine your hostname.\n");
	  exit(1);
	}
      myhost = strdup(h->h_name);
    }

  /* Now see who we're writing to. */
  if (argc < 2 || argc > 3)
    {
      fprintf(stderr, "Usage: write user [ttyname]\n");
      exit(1);
    }

  him = argv[1];
  hishost = strrchr(him, '@');
  if (!fromnet && hishost)
    {
      *hishost++ = '\0';
      h = gethostbyname(hishost);
      if (!h)
	{
	  struct in_addr addr;

	  if (inet_aton(hishost, &addr))
	    h = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);

	  if (!h)
	    {
	      fprintf(stderr, "write: unknown host: %s\n", hishost);
	      exit(1);
	    }
	}
      tonet = 1;
    }

  if (argc == 3)
    {
      /* User specified a tty. */
      histtyname = argv[2];

      /* If writing to a local user, make sure he's really logged in
       * on that tty, and that he's accepting messages.
       */
      if (!tonet)
	{
	  strncpy(line.ut_line, histtyname, sizeof(line.ut_line));
	  setutxent();
	  ut = getutxline(&line);
	  if (!ut || strncmp(ut->ut_name, him, sizeof(ut->ut_name)))
	    {
	      fprintf(stderr, "write: %s is not logged in on %s",
		      him, histtyname);
	      exit(1);
	    }

	  histty = malloc(6 + strlen(histtyname));
	  if (!histty)
	    {
	      perror("write: malloc failed");
	      exit(1);
	    }
	  sprintf(histty, "/dev/%s", histtyname);
	  if (!oktty(histty))
	    {
	      fprintf(stderr, "write: Permission denied.\n");
	      exit(1);
	    }
	}
    }
  else if (!tonet)
    {
      /* Find the tty(s) the user is logged in on. */
      int found = 0, nomsg = 0;

      histty = malloc(6 + sizeof(ut->ut_name));
      setutxent();
      while ((ut = getutxent()))
	{
	  if (!strncmp(him, ut->ut_name, sizeof(ut->ut_name)))
	    {
	      if (!found || nomsg)
		{
		  sprintf(histty, "/dev/%.*s", (int)sizeof(ut->ut_line),
			  ut->ut_line);
		  nomsg = !oktty(histty);
		}
	      found++;
	    }
	}

      if (!found)
	{
	  fprintf(stderr, "write: %s not logged in.\n", him);
	  exit(1);
	}
      else if (nomsg)
	{
	  fprintf(stderr, "write: Permission denied.\n");
	  exit(1);
	}
      else if (found > 1)
	{
	  fprintf(stderr, "write: %s logged in more than once... "
		  "writing to %s\n", him, histty + 5);
	}
    }


  /* Open connection to recipient. */
  if (tonet)
    {
      struct servent *s;
      struct sockaddr_in sin;

      s = getservbyname("write", "tcp");
      if (!s)
	{
	  fprintf(stderr, "write: unknown service write/tcp\n");
	  exit(1);
	}

      sin.sin_family = h->h_addrtype;
      memmove(&sin.sin_addr, h->h_addr, h->h_length);
      sin.sin_port = s->s_port;
      fd = socket(h->h_addrtype, SOCK_STREAM, 0);
      if (fd < 0)
	{
	  perror("write: socket creation failed");
	  exit(1);
	}
      if (connect(fd, (struct sockaddr *)&sin, sizeof (sin)) < 0)
	{
	  perror("write: Could not connect to remote host");
	  exit(1);
	}

      f = fdopen(fd, "r+");
      if (histtyname)
	fprintf(f, "%s@%s %s %s\r\n", me, myhost, him, histtyname);
      else
	fprintf(f, "%s@%s %s\r\n", me, myhost, him);
      fflush(f);

      /* Read response from server, which will be terminated with
       * an empty line.
       */
      while ((status = read_line(f, &buf, &bufsize)) == 0 && *buf)
	puts(buf);
      if (status != 0)
	exit(1);

      /* Some OSes do not allow you to change from input to output on
       * a FILE fdopen()ed "r+" without first explicitly setting the
       * file position.
       */
      rewind(f);
    }
  else
    {
      time_t now = time(0);
      struct tm *tm = localtime(&now);

      f = fopen(histty, "w");
      if (fromnet)
	{
	  printf("\n");
	  fflush(stdout);
	  fprintf(f, "\r\nMessage from %s on %s at %d:%02d...\r\n\007\007\007",
		  me, myhost, tm->tm_hour, tm->tm_min);
	}
      else
	{
	  fprintf(f, "\r\nMessage from %s@%s at %d:%02d...\r\n\007\007\007",
		  me, mytty, tm->tm_hour, tm->tm_min);
	}
      fflush(f);
    }

  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = eof;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  while (read_line(stdin, &buf, &bufsize) == 0)
    {
      char *p = buf;

      while (*p)
	{
	  if (!isascii((unsigned int)*p))
	    {
	      fputs("M-", f);
	      *p = toascii((unsigned int)*p);
	    }

	  if (isprint((unsigned int)*p) || isspace((unsigned int)*p))
	    putc(*p, f);
	  else
	    {
	      putc('^', f);
	      putc(*p ^ 0x40, f);
	    }

	  p++;
	}
      fputs("\r\n", f);
      fflush(f);

      if (ferror(f) || feof(f))
	{
	  fprintf(stderr, "\n\007write: failed (%s logged out?)\n", him);
	  exit(1);
	}
    }
  eof(0);
  /* NOTREACHED */
  exit(1);
}

static void eof(int sig)
{
  if (!tonet)
    {
      fputs("EOF\r\n", f);
      fflush(f);
    }
  exit(0);
}

static int oktty(char *tty)
{
  struct stat st;

  return access(tty, 0) == 0 && stat(tty, &st) == 0 && st.st_mode & S_IWGRP;
}


/* This is an internal function.  Its contract is to read a line from a
 * file into a dynamically allocated buffer, zeroing the trailing newline
 * if there is one.  The calling routine may call read_line multiple
 * times with the same buf and bufsize pointers; *buf will be reallocated
 * and *bufsize adjusted as appropriate.  The initial value of *buf
 * should be NULL.  After the calling routine is done reading lines, it
 * should free *buf.  This function returns 0 if a line was successfully
 * read, 1 if the file ended, and -1 if there was an I/O error or if it
 * ran out of memory.
 */

static int read_line(FILE *fp, char **buf, int *bufsize)
{
  char *newbuf;
  int offset = 0, len;

  if (*buf == NULL)
    {
      *buf = malloc(128);
      if (!*buf)
	return -1;
      *bufsize = 128;
    }

  while (1)
    {
      if (!fgets(*buf + offset, *bufsize - offset, fp))
	return (offset != 0) ? 0 : (ferror(fp)) ? -1 : 1;
      len = offset + strlen(*buf + offset);
      if ((*buf)[len - 1] == '\n')
	{
	  (*buf)[len - 1] = 0;
	  return 0;
	}
      offset = len;

      /* Allocate more space. */
      newbuf = realloc(*buf, *bufsize * 2);
      if (!newbuf)
	return -1;
      *buf = newbuf;
      *bufsize *= 2;
    }
}


#ifndef HAVE_GETUTXENT
static int utfd;

struct utmp *getutxent(void)
{
  static struct utmp ut;

  if (!utfd)
    setutxent();

  if (read(utfd, &ut, sizeof(ut)) != sizeof(ut))
    return NULL;
  return &ut;
}

struct utmp *getutxline(const struct utmp *line)
{
  struct utmp *ut;

  while ((ut = getutxent()))
    {
      if (!strncmp(line->ut_line, ut->ut_line, sizeof(line->ut_line)))
	return ut;
    }
  return NULL;
}

void setutxent(void)
{
  if (utfd)
    close(utfd);

  utfd = open(UTMP_FILE, O_RDONLY);
}
#endif

#ifndef HAVE_INET_ATON
#ifndef INADDR_NONE
#define	INADDR_NONE 0xffffffff
#endif

static int inet_aton(const char *str, struct in_addr *addr)
{
  addr->s_addr = inet_addr(str);
  return (addr->s_addr != INADDR_NONE || strcmp(str, "255.255.255.255") == 0);
}
#endif
