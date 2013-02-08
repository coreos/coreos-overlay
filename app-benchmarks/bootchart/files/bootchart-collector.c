/* bootchart-collector
 *
 * Copyright © 2009 Canonical Ltd.
 * Author: Scott James Remnant <scott@netsplit.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/resource.h>

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define BUFSIZE 524288


int append_buf (const char *str, size_t len,
		int outfd, char *outbuf, size_t *outlen);
int copy_buf (int fd, int outfd, char *outbuf, size_t *outlen);
int flush_buf (int outfd, char *outbuf, size_t *outlen);

int read_file (int fd, const char *uptime, size_t uptimelen,
	       int outfd, char *outbuf, size_t *outlen);
int read_proc (DIR *proc, const char *uptime, size_t uptimelen,
	       int outfd, char *outbuf, size_t *outlen);

unsigned long get_uptime (int fd);
void sig_handler (int signum);


int
append_buf (const char *str,
	    size_t      len,
	    int         outfd,
	    char       *outbuf,
	    size_t     *outlen)
{
	assert (len <= BUFSIZE);

	if (*outlen + len > BUFSIZE)
		if (flush_buf (outfd, outbuf, outlen) < 0)
			return -1;

	memcpy (outbuf + *outlen, str, len);
	*outlen += len;

	return 0;
}

int
copy_buf (int     fd,
	  int     outfd,
	  char   *outbuf,
	  size_t *outlen)
{
	for (;;) {
		ssize_t len;

		if (*outlen == BUFSIZE)
			if (flush_buf (outfd, outbuf, outlen) < 0)
				return -1;

		len = read (fd, outbuf + *outlen, BUFSIZE - *outlen);
		if (len < 0) {
			perror ("read");
			return -1;
		} else if (len == 0)
			break;

		*outlen += len;
	}

	return 0;
}

int
flush_buf (int     outfd,
	   char   *outbuf,
	   size_t *outlen)
{
	size_t writelen = 0;

	while (writelen < *outlen) {
		ssize_t len;

		len = write (outfd, outbuf + writelen, *outlen - writelen);
		if (len < 0) {
			perror ("write");
			exit (1);
		}

		writelen += len;
	}

	*outlen = 0;

	return 0;
}


int
read_file (int         fd,
	   const char *uptime,
	   size_t      uptimelen,
	   int         outfd,
	   char       *outbuf,
	   size_t     *outlen)
{
	lseek (fd, SEEK_SET, 0);

	if (append_buf (uptime, uptimelen, outfd, outbuf, outlen) < 0)
		return -1;

	if (copy_buf (fd, outfd, outbuf, outlen) < 0)
		return -1;

	if (append_buf ("\n", 1, outfd, outbuf, outlen) < 0)
		return -1;

	return 0;
}

int
read_proc (DIR        *proc,
	   const char *uptime,
	   size_t      uptimelen,
	   int         outfd,
	   char       *outbuf,
	   size_t     *outlen)
{
	struct dirent *ent;

	rewinddir (proc);

	if (append_buf (uptime, uptimelen, outfd, outbuf, outlen) < 0)
		return -1;

	while ((ent = readdir (proc)) != NULL) {
		char filename[PATH_MAX];
		int  fd;

		if ((ent->d_name[0] < '0') || (ent->d_name[0] > '9'))
			continue;

		sprintf (filename, "/proc/%s/stat", ent->d_name);

		fd = open (filename, O_RDONLY);
		if (fd < 0)
			continue;

		if (copy_buf (fd, outfd, outbuf, outlen) < 0)
			;

		if (close (fd) < 0)
			continue;
	}

	if (append_buf ("\n", 1, outfd, outbuf, outlen) < 0)
		return -1;

	return 0;
}


unsigned long
get_uptime (int fd)
{
	char          buf[80];
	ssize_t       len;
	unsigned long u1, u2;

	lseek (fd, SEEK_SET, 0);

	len = read (fd, buf, sizeof buf);
	if (len < 0) {
		perror ("read");
		return 0;
	}

	buf[len] = '\0';

	if (sscanf (buf, "%lu.%lu", &u1, &u2) != 2) {
		perror ("sscanf");
		return 0;
	}

	return u1 * 100 + u2;
}


void
sig_handler (int signum)
{
}

int
main (int   argc,
      char *argv[])
{
	struct sigaction  act;
	sigset_t          mask, oldmask;
	struct rlimit     rlim;
	struct timespec   timeout;
	const char       *output_dir = ".";
	char              filename[PATH_MAX];
	int               sfd, dfd, ufd;
	DIR              *proc;
	int               statfd, diskfd, procfd;
	char              statbuf[BUFSIZE], diskbuf[BUFSIZE], procbuf[BUFSIZE];
	size_t            statlen = 0, disklen = 0, proclen = 0;
	unsigned long     reltime = 0;
	int               arg = 1, rel = 0;

	if ((argc > arg) && (! strcmp (argv[arg], "-r"))) {
		rel = 1;
		arg++;
	}

	if (argc <= arg) {
		fprintf (stderr, "Usage: %s [-r] HZ [DIR]\n", argv[0]);
		exit (1);
	}

	if (argc > arg) {
		unsigned long  hz;
		char          *endptr;

		hz = strtoul (argv[arg], &endptr, 10);
		if (*endptr) {
			fprintf (stderr, "%s: HZ not an integer\n", argv[0]);
			exit (1);
		}

		if (hz > 1) {
			timeout.tv_sec = 0;
			timeout.tv_nsec = 1000000000 / hz;
		} else {
			timeout.tv_sec = 1;
			timeout.tv_nsec = 0;
		}

		arg++;
	}

	if (argc > arg) {
		output_dir = argv[arg];
		arg++;
	}


	sigemptyset (&mask);
	sigaddset (&mask, SIGTERM);
	sigaddset (&mask, SIGINT);

	if (sigprocmask (SIG_BLOCK, &mask, &oldmask) < 0) {
		perror ("sigprocmask");
		exit (1);
	}

	act.sa_handler = sig_handler;
	act.sa_flags = 0;
	sigemptyset (&act.sa_mask);

	if (sigaction (SIGTERM, &act, NULL) < 0) {
		perror ("sigaction SIGTERM");
		exit (1);
	}

	if (sigaction (SIGINT, &act, NULL) < 0) {
		perror ("sigaction SIGINT");
		exit (1);
	}

	/* Drop cores if we go wrong */
	if (chdir ("/"))
		;

	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;

	setrlimit (RLIMIT_CORE, &rlim);


	proc = opendir ("/proc");
	if (! proc) {
		perror ("opendir /proc");
		exit (1);
	}

	sfd = open ("/proc/stat", O_RDONLY);
	if (sfd < 0) {
		perror ("open /proc/stat");
		exit (1);
	}

	dfd = open ("/proc/diskstats", O_RDONLY);
	if (dfd < 0) {
		perror ("open /proc/diskstats");
		exit (1);
	}

	ufd = open ("/proc/uptime", O_RDONLY);
	if (ufd < 0) {
		perror ("open /proc/uptime");
		exit (1);
	}


	sprintf (filename, "%s/proc_stat.log", output_dir);
	statfd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (statfd < 0) {
		perror ("open proc_stat.log");
		exit (1);
	}

	sprintf (filename, "%s/proc_diskstats.log", output_dir);
	diskfd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (diskfd < 0) {
		perror ("open proc_diskstats.log");
		exit (1);
	}

	sprintf (filename, "%s/proc_ps.log", output_dir);
	procfd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (procfd < 0) {
		perror ("open proc_ps.log");
		exit (1);
	}


	if (rel) {
		reltime = get_uptime (ufd);
		if (! reltime)
			exit (1);
	}

	for (;;) {
		char          uptime[80];
		size_t        uptimelen;
		unsigned long u;

		u = get_uptime (ufd);
		if (! u)
			exit (1);

		uptimelen = sprintf (uptime, "%lu\n", u - reltime);


		if (read_file (sfd, uptime, uptimelen,
			       statfd, statbuf, &statlen) < 0)
			exit (1);

		if (read_file (dfd, uptime, uptimelen,
			       diskfd, diskbuf, &disklen) < 0)
			exit (1);

		if (read_proc (proc, uptime, uptimelen,
			       procfd, procbuf, &proclen) < 0)
			exit (1);

		if (pselect (0, NULL, NULL, NULL, &timeout, &oldmask) < 0) {
			if (errno == EINTR) {
				break;
			} else {
				perror ("pselect");
				exit (1);
			}
		}
	}


	if (flush_buf (statfd, statbuf, &statlen) < 0)
		exit (1);
	if (close (statfd) < 0) {
		perror ("close proc_stat.log");
		exit (1);
	}

	if (flush_buf (diskfd, diskbuf, &disklen) < 0)
		exit (1);
	if (close (diskfd) < 0) {
		perror ("close proc_diskstats.log");
		exit (1);
	}

	if (flush_buf (procfd, procbuf, &proclen) < 0)
		exit (1);
	if (close (procfd) < 0) {
		perror ("close proc_ps.log");
		exit (1);
	}


	if (close (ufd) < 0) {
		perror ("close /proc/uptime");
		exit (1);
	}

	if (close (dfd) < 0) {
		perror ("close /proc/diskstats");
		exit (1);
	}

	if (close (sfd) < 0) {
		perror ("close /proc/stat");
		exit (1);
	}

	if (closedir (proc) < 0) {
		perror ("close /proc");
		exit (1);
	}

	return 0;
}
