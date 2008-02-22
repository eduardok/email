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
#ifndef __EMAIL_H
#define __EMAIL_H   1

/* Everyone needs this */
#include <assert.h>

#include <dlist.h>
#include <dhash.h>

#define MINBUF  300
#define MAXBUF  600
#define ERROR   -6
#define TRUE    1
#define FALSE   0
#define EASY    100

/* EMAIL_DIR determined at compile time */
#define MAIN_CONFIG  EMAIL_DIR"/email.conf"
#define EMAIL_HELP_FILE   EMAIL_DIR"/email.help"

/* Debugger */
#define DEBUG(str)                             \
(                                              \
  fprintf(stderr, "DEBUG: %s:%s():%d: %s\n",   \
      __FILE__, __FUNCTION__, __LINE__, (str)) \
)

struct addr {
	char *name;
	char *email;
};

/* Globally defined vars */
dhash table;
char *conf_file;
short quiet;

struct mailer_options {
	short html;
	short encrypt;
	short sign;
	short priority;
	short receipt;
	short blank;
	char *subject;
	dlist attach;
	dlist headers;
	dlist to;
	dlist cc;
	dlist bcc;
} Mopts;

void usage(void);

#define setConfValue(tok, val) (dhInsert(table, (tok), (val)))
#define getConfValue(tok) ((char *)dhGetItem(table, (tok)))

#endif /* __EMAIL_H */
