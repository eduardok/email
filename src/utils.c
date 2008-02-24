/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>

/* Autoconf manual suggests this. */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "email.h"
#include "utils.h"
#include "error.h"

/**
 * takes a string that is a supposed file path, and 
 * checks for certian wildcards ( like ~ for home directory ) 
 * and resolves to an actual absolute path.
**/
dstrbuf *
expandPath(const char *path)
{
	struct passwd *pw = NULL;
	dstrbuf *tmp = DSB_NEW;
	dstrbuf *ret = DSB_NEW;

	dsbCopy(tmp, path);

	if (tmp->len > 0 && tmp->str[0] == '&') {
		dsbCopy(ret, EMAIL_DIR);
	} else if (tmp->len > 0 && tmp->str[0] == '~') {
		if (tmp->str[1] == '/') {
			pw = getpwuid(getuid());
		} else {
			int pos = strfind(tmp->str, '/');
			if (pos >= 0) {
				char *p = substr(tmp->str, 1, pos-1);
				if (p) {
					pw = getpwnam(p);
					xfree(p);
				}
			}
			if (!pw) {
				pw = getpwuid(getuid());
			}
		}
		if (pw) {
			dsbCopy(ret, pw->pw_dir);
		}
	}

	
	if (ret->len > 0) {
		int pos = strfind(tmp->str, '/');
		if (pos > 0) {
			char *p = substr(tmp->str, pos, tmp->len);
			if (p) {
				dsbCat(ret, p);
				xfree(p);
			}
		}
	} else {
		dsbCopy(ret, path);
	}

	dsbDestroy(tmp);
	return ret;
}

int
copyfile(const char *from, const char *to)
{
	FILE *ffrom, *fto;
	dstrbuf *buf=NULL;

	ffrom = fopen(from, "r");
	fto = fopen(to, "w");
	if (!ffrom || !fto) {
		return -1;
	}

	buf = DSB_NEW;
	while (dsbFread(buf, MAXBUF, ffrom) > 0) {
		fwrite(buf->str, sizeof(char), buf->len, fto);
		dsbClear(buf);
	}

	dsbDestroy(buf);
	fclose(ffrom);
	fclose(fto);

	if (ferror(ffrom) || ferror(fto)) {
		return -1;
	}

	return 0;
}

/**
 * checks to see if the TEMP_FILE is around... if it is
 * it will move it to the users home directory as dead.letter.
**/
void
deadLetter(dstrbuf *msg)
{
	dstrbuf *path = expandPath("~/dead.letter");
	FILE *out = fopen(path->str, "w");

	if (!out) {
		warning("Could not save dead letter to %s", path->str);
	} else {
		fwrite(msg->str, sizeof(char), msg->len, out);
	}
	dsbDestroy(path);
}



/**
 * Gererate a string of random characters
**/
static const char letters[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz" 
                              "0123456789";

#define SIZEOF_LETTERS 62

dstrbuf *
randomString(size_t size)
{
	size_t i;
	long randval;
	struct timeval mill;
	dstrbuf *ret = DSB_NEW;

	gettimeofday(&mill, NULL);
	srand((getuid() + getpid()) + (mill.tv_usec << 16));

	for (i=0; i < size; i++) {
		randval = rand() / (RAND_MAX / SIZEOF_LETTERS);
		dsbnCat(ret, &letters[randval], 1);
	}
	return ret;
}

/**
 * Get the first element from the Mopts.to list of emails
 * and return it without the name or formating. just the
 * email address itself.
**/
dstrbuf *
getFirstEmail(void)
{
	char *tmp=NULL;
	dstrbuf *buf = DSB_NEW;
	struct addr *a = (struct addr *)dlGetTop(Mopts.to);

	assert(a != NULL);
	
	/* If we haven't found a <, consider the e-mail unformatted. */
	tmp = strchr(a->email, '<');
	if (!tmp) {
		tmp = a->email;
	} else {
		/* strchr only brings us to the '<', Get past it */
		++tmp;
	}

	dsbCopy(buf, tmp);
	tmp = strchr(buf->str, '>');
	if (tmp) {
		*tmp = '\0';
	}

	return buf;
}

/**
 * Exit just handles all the signals and exiting of the 
 * program by freeing the allocated memory  and writing 
 * the dead.letter if we had a sudden interrupt...
**/
void
properExit(int sig)
{
	if (sig != 0) {
		//deadLetter();
	}

	/* Free lists */
	if (Mopts.attach) {
		dlDestroy(Mopts.attach);
	}
	if (Mopts.headers) {
		dlDestroy(Mopts.headers);
	}
	if (Mopts.to) {
		dlDestroy(Mopts.to);
	}
	if (Mopts.cc) {
		dlDestroy(Mopts.cc);
	}
	if (Mopts.bcc) {
		dlDestroy(Mopts.bcc);
	}

	dhDestroy(table);
	exit(sig);
}

int
copyUpTo(dstrbuf *buf, int stop, FILE *in)
{
	int ch;
	while ((ch = fgetc(in)) != EOF) {
		if (ch == '\\') {
			ch = fgetc(in);
			if (ch == '\r') {
				ch = fgetc(in);
			}
			if (ch == '\n') {
				continue;
			}
		}
		if (ch == '\r') {
			ch = fgetc(in);
			if (ch == '\n') {
				return ch;
			}
		}
		if (ch == stop) {
			return ch;
		}
		dsbnCat(buf, (char *)&ch, 1);
	}
	return ch;
}

