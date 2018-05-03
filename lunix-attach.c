/*
 * lunix-attach.c
 *
 * Make the Lunix:TNG driver receive data from the specified
 * TTY, by attaching the Lunix line discipline to it.
 *
 * Based on slattach.c for SLIP operation
 * [net-tools Debian package].
 *
 * Must be run with root privilege.
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#include <pwd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>          
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "lunix.h"

#ifndef _PATH_LOCKD
#define _PATH_LOCKD		"/var/lock"		/* lock files   */
#endif
#ifndef _UID_UUCP
#define _UID_UUCP		"uucp"			/* owns locks   */
#endif

struct {
	const char *speed;
	int code;
} tty_speeds[] = {			/* table of usable baud rates	*/
  { "50",	B50	}, { "75",	B75  	},	
  { "110",	B110	}, { "300",	B300	},
  { "600",	B600	}, { "1200",	B1200	},
  { "2400",	B2400	}, { "4800",	B4800	},
  { "9600",	B9600	},
#ifdef B14400
  { "14400",	B14400	},
#endif
#ifdef B19200
  { "19200",	B19200	},
#endif
#ifdef B38400
  { "38400",	B38400	},
#endif
#ifdef B57600
  { "57600",	B57600	},
#endif
#ifdef B115200
  { "115200",	B115200	},
#endif
  { NULL,	0	}
};

/*
 * Global data
 *
 */
int tty_fd = -1;
struct termios tty_before, tty_current;
int ldisc_before;

/* Check for an existing lock file on our device */
static int tty_already_locked(char *nam)
{
	int  i = 0, pid = 0;
	FILE *fd = 0;

	/* Does the lock file on our device exist? */
	if ((fd = fopen(nam, "r")) == NULL)
		return 0; /* No, return perm to continue */
  
	/*
	 * Yes, the lock is there.  Now let's make sure 
	 * at least there's no active process that owns
	 * that lock.
	 */
	i = fscanf(fd, "%d", &pid);
	(void) fclose(fd);
 
	if (i != 1) /* Lock file format's wrong! Kill't */
		return 0;

	/* We got the pid, check if the process's alive */
	if (kill(pid, 0) == 0)      /* it found process */
		return 1;          /* Yup, it's running... */

	/* Dead, we can proceed with locking this device...  */
	return 0;
}

/* Lock or unlock a terminal line. */
static int tty_lock(char *path, int mode)
{
	int fd;
	int ret;
	char apid[16];
	struct passwd *pw;
	static int saved_lock = 0;
	static char saved_path[PATH_MAX];

	/* We do not lock standard input. */
	if (mode == 1) {	/* lock */
		sprintf(saved_path, "%s/LCK..%s", _PATH_LOCKD, path);
		if (tty_already_locked(saved_path)) {
			fprintf(stderr, "/dev/%s already locked\n", path);
			return -1;
		}
		if ((fd = creat(saved_path, 0644)) < 0) {
			if (errno != EEXIST) {
				fprintf(stderr, "tty_lock: (%s): %s\n",
						saved_path, strerror(errno));
			}
			return -1;
		}
		sprintf(apid, "%10d\n", getpid());
		if ((ret = write(fd, apid, strlen(apid))) != strlen(apid)) {
			fprintf(stderr, "write to PID file incomplete, ret = %d\n", ret);
			close(fd);
			unlink(saved_path);
			return -1;
		}
		(void) close(fd);

		/* Make sure UUCP owns the lockfile.  Required by some packages. */
		if ((pw = getpwnam(_UID_UUCP)) == NULL) {
			fprintf(stderr, "tty_lock: UUCP user %s unknown\n", _UID_UUCP);
			return 0;
		}
		(void) chown(saved_path, pw->pw_uid, pw->pw_gid);
		saved_lock = 1;
	} else {	/* unlock */
		if (saved_lock != 1)
			return 0;
		if (unlink(saved_path) < 0) {
			fprintf(stderr, "tty_unlock: (%s): %s\n",
				saved_path, strerror(errno));
			return -1;
		}
		saved_lock = 0;
	}
	
	return 0;
}

/* Find a serial speed code in the table. */
static int tty_find_speed(const char *speed)
{
	int i;

	i = 0;
	while (tty_speeds[i].speed != NULL) {
		if (!strcmp(tty_speeds[i].speed, speed)) return(tty_speeds[i].code);
		i++;
	}

	return -EINVAL;
}

/* Set the number of stop bits. */
static int tty_set_stopbits(struct termios *tty, char *stopbits)
{
	switch(*stopbits) {
	case '1':
		tty->c_cflag &= ~CSTOPB;
		break;

	case '2':
		tty->c_cflag |= CSTOPB;
		break;

	default:
		return -EINVAL;
  	}

	return 0;
}

/* Set the number of data bits. */
static int tty_set_databits(struct termios *tty, char *databits)
{
	tty->c_cflag &= ~CSIZE;
	switch(*databits) {
	case '5':
		tty->c_cflag |= CS5;
		break;

	case '6':
		tty->c_cflag |= CS6;
		break;

	case '7':
		tty->c_cflag |= CS7;
		break;

	case '8':
		tty->c_cflag |= CS8;
		break;

	default:
		return -EINVAL;
	}  	

	return 0;
}

/* Set the type of parity encoding. */
static int tty_set_parity(struct termios *tty, char *parity)
{
	switch(toupper(*parity)) {
	case 'N':
		tty->c_cflag &= ~(PARENB | PARODD);
		break;  

	case 'O':
		tty->c_cflag &= ~(PARENB | PARODD);
		tty->c_cflag |= (PARENB | PARODD);
		break;

	case 'E':
		tty->c_cflag &= ~(PARENB | PARODD);
		tty->c_cflag |= (PARENB);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}


/* Set the line speed of a terminal line. */
static int tty_set_speed(struct termios *tty, const char *speed)
{
	int code;
	if ((code = tty_find_speed(speed)) < 0)
		return code;
	tty->c_cflag &= ~CBAUD;
	tty->c_cflag |= code;

	return 0;
}


/* Put a terminal line in a transparent state. */
static int tty_set_raw(struct termios *tty)
{
	int i;
	int speed;
	
	for (i = 0; i < NCCS; i++)
		tty->c_cc[i] = '\0';		/* no spec chr		*/
	tty->c_cc[VMIN] = 1;
	tty->c_cc[VTIME] = 0;
	tty->c_iflag = (IGNBRK | IGNPAR);	/* input flags		*/
	tty->c_oflag = (0);			/* output flags		*/
	tty->c_lflag = (0);			/* local flags		*/
	speed = (tty->c_cflag & CBAUD);		/* save current speed	*/
	tty->c_cflag = (CRTSCTS | HUPCL | CREAD); /* UART flags		*/
	tty->c_cflag |= CLOCAL;
	tty->c_cflag |= speed;			/* restore speed	*/
	
	return 0;
}


/* Fetch the state of a terminal. */
static int tty_get_state(struct termios *tty)
{
	int saved_errno;

	if (ioctl(tty_fd, TCGETS, tty) < 0) {
		saved_errno = errno;
		perror("Get TTY State:");
		return -saved_errno;
	}
	
	return 0;
}

/* Set the state of a terminal. */
static int tty_set_state(struct termios *tty)
{
	int saved_errno;

	if (ioctl(tty_fd, TCSETS, tty) < 0) {
		saved_errno = errno;
		perror("Set TTY State:");
		return -saved_errno;
  	}

	return 0;
}

/* Get the TTY line discipline. */
static int tty_get_ldisc(int *disc)
{
	int saved_errno;

	if (ioctl(tty_fd, TIOCGETD, disc) < 0) {
		saved_errno = errno;
		perror("get ldisc: failed to get line discipline");
		fprintf(stderr, "Is the Lunix:TNG discipline actually loaded?!\n");
		return -saved_errno;
	}
	
	return 0;
}

/* Set the TTY line discipline. */
static int tty_set_ldisc(int disc)
{
	int saved_errno;

	if (ioctl(tty_fd, TIOCSETD, &disc) < 0) {
		saved_errno = errno;
		perror("set ldisc: failed to set line discipline");
		return -saved_errno;
	}
	
	return 0;
}

/* Restore the TTY to its previous state. */
static int tty_restore(void)
{
	int ret;
	struct termios tty;

	tty = tty_before;
  	(void) tty_set_speed(&tty, "0");
	if ((ret = tty_set_state(&tty)) < 0) {
		fprintf(stderr, "slattach: tty_restore: %s\n",
			strerror(-ret));
		return ret;
	}
  	
	return 0;
}

/* Close down a terminal line. */
static int tty_close(void)
{
	/*
	 * Set the old discipline and restore the
	 * previous line mode.
	 */
	(void) tty_set_ldisc(ldisc_before);
	(void) tty_restore();
	(void) tty_lock(NULL, 0);

	return 0;
}

/* Open and initialize a terminal line. */
static int tty_open(char *name)
{
	int fd;
	int ret;
	int saved_errno;
	char pathbuf[PATH_MAX];
	register char *path_open, *path_lock;

	/* Try opening the TTY device. */
	if (name != NULL) {
		if (name[0] != '/') {
			if (strlen(name + 6) > sizeof(pathbuf)) {
				fprintf(stderr, "tty name too long\n");
				return -1;
			}
			sprintf(pathbuf, "/dev/%s", name);
			path_open = pathbuf;
			path_lock = name;
		} else if (!strncmp(name, "/dev/", 5)) {
			path_open = name;
			path_lock = name + 5;
		} else {
			path_open = name;
			path_lock = name;
		}
	
		fprintf(stderr, "tty_open: looking for lock\n");
		if (tty_lock(path_lock, 1))
			return -1 ; /* can we lock the device? */
		fprintf(stderr, "tty_open: trying to open %s\n",
			path_open);
		if ((fd = open(path_open, O_RDWR|O_NDELAY)) < 0) {
			saved_errno = errno;
			fprintf(stderr, "tty_open(%s, RW): %s\n",
				path_open, strerror(errno));
			return -saved_errno;
		}
		tty_fd = fd;
		fprintf(stderr, "tty_open: %s (fd=%d) ", path_open, fd);
  	} else {
		tty_fd = 0;
	}

	/* Fetch the current state of the terminal. */
	if (tty_get_state(&tty_before) < 0) {
		saved_errno = errno;
		fprintf(stderr, "tty_open: cannot get current state\n");
		return -saved_errno;
	}
	tty_current = tty_before;
	
	/* Fetch the current line discipline of this terminal. */
	if (tty_get_ldisc(&ldisc_before) < 0) {
		saved_errno = errno;
		fprintf(stderr, "tty_open: cannot get current line disc\n");
		return -saved_errno;
	}

	/* Put this terminal line in a 8-bit transparent mode. */
	if (tty_set_raw(&tty_current) < 0) {
		saved_errno = errno;
		fprintf(stderr, "tty_open: cannot set RAW mode\n");
		return -saved_errno;
	}

	/**************************************************
	 * The sensor needs to be setup at
	 * 57600bps, 8 data bits, No parity, 1 stop bit:
	 **************************************************
	 */
	if (tty_set_speed(&tty_current, "57600") != 0) {
			saved_errno = errno;
			fprintf(stderr, "tty_open: cannot set data rate to 57600bps\n");
			return -saved_errno;
	}
	if (tty_set_databits(&tty_current, "8") ||
	    tty_set_stopbits(&tty_current, "1") ||
	    tty_set_parity(&tty_current, "N")) {
	    	saved_errno = errno;
		fprintf(stderr, "tty_open: cannot set 8N1 mode\n");
		return -saved_errno;
  	};

	/* Set the new line mode. */
	if ((ret = tty_set_state(&tty_current)) < 0)
		return ret;

	/* And activate the new line discipline */
	if ((ret = tty_set_ldisc(N_LUNIX_LDISC)) < 0)
		return ret;
		
	return 0;
}

/* Catch any signals. */
static void sig_catch(int sig)
{
	tty_close();
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr,
			"Usage: %s tty_line\n"
			"where tty_line is the TTY on which to set the Lunix line discipline.\n\n",
			argv[0]);
		exit(1);
	}
	
	if (tty_open(argv[1]) < 0)
		return 1;
	
	fprintf(stderr, "Line discipline set on %s, press ^C to release the TTY...\n",
		argv[1]);
	
  	(void) signal(SIGHUP, sig_catch);
  	(void) signal(SIGINT, sig_catch);
  	(void) signal(SIGQUIT, sig_catch);
  	(void) signal(SIGTERM, sig_catch);
	
	while (pause())
		;

	/* Unreachable */
	return 100;
}
