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
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "email.h"
#include "ipfunc.h"
#include "utils.h"

#define SA struct sockaddr


static struct hostent *xgethostbyname(const char *);

/**
 * Gethostbyname is a wrapper of gethostbyname2().    It 
 * will return the standard hostent structure to the user, 
 * or NULL if a failure occurs.
**/

static struct hostent *
xgethostbyname(const char *host)
{
    struct hostent *him;

#if defined (IPV6)
    him = gethostbyname2(host, AF_INET6);
#else
    him = gethostbyname(host);
#endif

    return (him);
}

/**
 * Socket_connect will open a socket and connect to the hostname/ip
 * passed to it.    It will return the socket that is currently 
 * connected.    The SOCKET structure which is defined in ipfunc.h
 * will be used for buffering such as the FILE structure is.
**/

SOCKET *
socket_connect(const char *hostname, int port)
{
    SOCKET *sd;
    struct sockaddr_in serv;
    struct hostent *him;

    /* Initialize Socket struct */
    sd = xmalloc(sizeof(SOCKET));
    him = xgethostbyname(hostname);
    if (!him)
        return (NULL);

    memset(&serv, 0, sizeof(struct sockaddr_in));
    serv.sin_family = PF_INET;
    serv.sin_port = htons(port);
    memcpy(&serv.sin_addr.s_addr, him->h_addr_list[0], sizeof(serv.sin_addr.s_addr));

    sd->sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sd->sock < 0)
        return (NULL);

    if (connect(sd->sock, (SA *) & serv, sizeof(serv)) < 0)
        return (NULL);

    return (sd);
}

/**
 * Close the socket connection and destroy the SOCKET structure
**/

void
socket_close(SOCKET * sd)
{
    if (!sd)
        return;

    if (sd->sock)
        close(sd->sock);

    free(sd);
}

/**
 * This function will be similar to fgetc except it will
 * use the SOCKET structure for buffering.    It will read
 * up to sizeof(sd->buf) bytes into the internal buffer
 * and then let sd->bufptr point to it.    If bufptr is 
 * not NULL, then it will return a byte each time it is
 * called and advance the pointer for the next call. If
 * bufptr is NULL, it will read another sizeof(sd->buf)
 * bytes and reset sd->bufptr.
**/

int
sgetc(SOCKET * sd)
{
    int retval;

    assert(sd != NULL);

    /* If there are some available bytes, send them */
    if (sd->avail_bytes > 0) {
        sd->avail_bytes--;
        return (*sd->bufptr++);
    }

    retval = recv(sd->sock, sd->buf, sizeof(sd->buf), 0);
    if (retval == 0) {
        sd->flags |= SOCKET_EOF;
        return (EOF);
    }
    else if (retval == -1) {
        sd->flags |= SOCKET_ERROR;
        sd->errnum = errno;
        return (-1);
    }

    sd->bufptr = sd->buf;
    sd->avail_bytes = retval - 1;       /* because we're about to send one byte */

    return (*sd->bufptr++);
}

/**
 * This function will be similar to fputc except it will
 * use the SOCKET struture instead of the FILE structure
 * to place the file on the stream.
**/

int
sputc(int ch, SOCKET * sd)
{
    char buf[2] = { 0 };

    assert(sd != NULL);

    snprintf(buf, 2, "%c", ch);
    if (send(sd->sock, buf, 1, 0) != 1) {
        sd->flags |= SOCKET_ERROR;
        sd->errnum = errno;
        return (-1);
    }

    return (0);
}

/**
 * This function will be similar to fgets except it will
 * use the Sgetc to read one character at a time.    It 
 * will NOT return \n or \r like fgets does.
**/

char *
sgets(char *buf, size_t size, SOCKET * sd)
{
    u_int i;
    int ch;

    /* start i at 1 to read size - 1 */
    for (i = 1; i < size; i++) {
        ch = sgetc(sd);
        if (ch == -1)
            return (NULL);
        else if (ch == EOF)
            break;
        else if (ch == '\r')
            continue;
        else if (ch == '\n')
            break;

        *buf++ = ch;
    }

    *buf = '\0';
    return (buf);
}

/**
 * This function will take a SOCKET and perform
 * functionality similar to fputs().    If you want to 
 * know more, please read man fputs.
**/

int
sputs(const char *buf, SOCKET * sd)
{
    int bytes = 0;
    u_int current_size = 0;
    u_int size = strlen(buf);
    u_int size_left = size;

    while (current_size < size) {
        bytes = send(sd->sock, buf + current_size, size_left, 0);
        if (bytes == -1) {
            sd->flags |= SOCKET_ERROR;
            sd->errnum = errno;
            return (-1);
        }

        current_size += bytes;
        size_left -= bytes;
    }

    return (0);
}

/**
    Writes to a socket
**/

int
ssend(const char *buf, size_t len, SOCKET * sd)
{
    int bytes = 0;
    const size_t blocklen = 4356;

    while (len > 0) {
        bytes = send(sd->sock, buf, (len > blocklen) ? blocklen : len, 0);
        if (bytes == -1) {
            if (errno == EAGAIN)
                continue;
            if (errno == EINTR)
                continue;
            sd->flags |= SOCKET_ERROR;
            sd->errnum = errno;
            return (-1);
        }
        if (bytes == 0) {
            sd->flags |= SOCKET_ERROR;
            sd->errnum = EPIPE;
        }
        if (bytes > 0) {
            buf += bytes;
            len -= bytes;
        }
    }

    return (0);
}

/**
 * Will test the flags for a SOCKET_ERROR 
**/

int
serror(SOCKET * sd)
{
    return (sd->flags & SOCKET_ERROR);
}

/**
 * Will test the flags for a SOCKET_EOF
**/

int
seof(SOCKET * sd)
{
    return (sd->flags & SOCKET_EOF);
}


/**
    Returns the error string from the system which is
    determined by errnum (errno).
**/

char *
socket_get_error(SOCKET * sd)
{
    return (strerror(sd->errnum));
}
