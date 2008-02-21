/**

    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2004 email by Dean Jones
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
#ifndef IPFUNC_H
#define IPFUNC_H 1

#include <netdb.h>

typedef struct socket {
    int sock;                   /* Socket descriptor */
    int flags;                  /* Error flags */
    int errnum;                 /* Essentially, errno */
    int avail_bytes;            /* Available bytes in buffer */
    char buf[500];              /* Internal buffer */
    char *bufptr;               /* Pointer to Internal buffer */

} SOCKET;

/* Error flags */
#define SOCKET_ERROR 0x01
#define SOCKET_EOF   0x02

SOCKET *socket_connect(const char *, int);
void socket_close(SOCKET *);
int sgetc(SOCKET *);
int sputc(int, SOCKET *);
char *sgets(char *, size_t, SOCKET *);
int sputs(const char *, SOCKET *);
int ssend(const char *, size_t, SOCKET *);
int serror(SOCKET *);
int seof(SOCKET *);
char *socket_get_error(SOCKET *);

#endif /* IPFUNC_H */
