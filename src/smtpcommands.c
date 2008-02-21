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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "email.h"
#include "ipfunc.h"

#include "smtpcommands.h"
#include "utils.h"
#include "progress_bar.h"
#include "mimeutils.h"
#include "error.h"

static char error_string[MINBUF + 1];

static void set_smtp_error(char *);
static int readresponse(SOCKET *, char *, size_t);
static int writeresponse(SOCKET *, char *, ...);
static int helo(SOCKET *, const char *);
static int ehlo(SOCKET *, const char *);
static int mailfrom(SOCKET *, const char *);
static int rcpt(SOCKET *, char *);
static int data(SOCKET *);
static int send_data(SOCKET *, FILE *);
static int quit(SOCKET *);
static int rset(SOCKET *);
static int smtp_auth_login(SOCKET *, const char *, const char *);
static int smtp_auth_plain(SOCKET *, const char *, const char *);

/**
 * Will generate a string from an error code and return
 * that string as a return value.
**/

char *
smtp_errstring(void)
{
    return (error_string);
}

/**
 * Simple interface to copy over buffer into error string
**/

static void
set_smtp_error(char *buf)
{
    memset(error_string, '\0', sizeof(error_string));
    memcpy(error_string, buf, sizeof(error_string));
}

/**
 * Reads a line from the smtp server using sgets().
 * It will continue to read the response until the 4th character
 * in the line is found to be a space.    Per the RFC 821 this means
 * that this will be the last line of response from the SMTP server
 * and the appropirate return value response.
**/

static int
readresponse(SOCKET * sd, char *buf, size_t size)
{
    int retval;
    char val[5] = { 0 };

    do {
        sgets(buf, size, sd);
        if (serror(sd) || seof(sd)) {
            set_smtp_error("Lost connection with SMTP server");
            return (ERROR);
        }

        memcpy(val, buf, 4);

    } while (val[3] != ' ');
    /* The last line of a response has a space in the 4th column */

    retval = atoi(val);
    return (retval);
}

static int
writeresponse(SOCKET * sd, char *line, ...)
{
    va_list vp;
    char buf[MINBUF] = { 0 };

    va_start(vp, line);
    vsnprintf(buf, MINBUF - 1, line, vp);
    va_end(vp);

    sputs(buf, sd);
    if (serror(sd))
        return (-1);

    return (0);
}

/**
 * Initializes the SMTP communications with SMTP_AUTH.
**/

int
init_smtp_auth(SOCKET * sd, const char *auth, const char *user, const char *pass)
{
    if (strcasecmp(auth, "LOGIN") == 0) {
        if (smtp_auth_login(sd, user, pass) == ERROR)
            return (ERROR);
    }
    else if (strcasecmp(auth, "PLAIN") == 0) {
        if (smtp_auth_plain(sd, user, pass) == ERROR)
            return (ERROR);
    }
    else {
        warning("SMTP_AUTH was invalid.  Trying AUTH type of LOGIN...");
        if (smtp_auth_login(sd, user, pass) == ERROR)
            return (ERROR);
    }

    return (0);
}


/**
 * Initializes the SMTP communications by sending the EHLO
 * command.    If EHLO fails, it will send HELO.    It will then
 * send the MAIL FROM: command.
**/

int
init_smtp(SOCKET * sd, const char *domain)
{
    int retval;

    retval = ehlo(sd, domain);
    if (retval == ERROR) {
        /* Per RFC, can ignore RSET, if error. */
        rset(sd);
        retval = helo(sd, domain);
        if (retval == ERROR)
            return (ERROR);
    }

    return (0);
}

/**
 * Sets who the message is from.  Basically runs the 
 * MAIL FROM: SMTP command
**/

int
set_smtp_mail_from(SOCKET * sd, const char *from_email)
{
    int retval;

    retval = mailfrom(sd, from_email);
    if (retval == ERROR)
        return (ERROR);

    return (0);
}

/**
 * Sets all the recipients with the RCPT command
 * to the smtp server.
**/

int
set_smtp_recipients(SOCKET * sd, list_t to, list_t cc, list_t bcc)
{
    int retval;
    list_t next = NULL;
    list_t saved = NULL;

    next = list_getnext(to, &saved);
    while (next) {
        retval = rcpt(sd, next->b_data);
        if (retval == ERROR)
            return (ERROR);
        next = list_getnext(NULL, &saved);
    }

    next = list_getnext(cc, &saved);
    while (next) {
        retval = rcpt(sd, next->b_data);
        if (retval == ERROR)
            return (ERROR);
        next = list_getnext(NULL, &saved);
    }

    next = list_getnext(bcc, &saved);
    while (next) {
        retval = rcpt(sd, next->b_data);
        if (retval == ERROR)
            return (ERROR);
        next = list_getnext(NULL, &saved);
    }

    return (0);
}

/**
 * Sends all the data to the smtp server
**/

int
send_smtp_data(SOCKET * sd, FILE * email)
{
    int retval;

    retval = data(sd);
    if (retval == ERROR)
        return (ERROR);

    rewind(email);
    retval = send_data(sd, email);
    if (retval == ERROR)
        return (ERROR);

    return (0);
}

/**
 * Sends the QUIT\r\n signal to the smtp server and
 * closes the link.
**/

int
close_smtp_connection(SOCKET * sd)
{
    int retval;

    retval = quit(sd);
    if (retval == ERROR)
        return (ERROR);

    return (0);
}


static int
helo(SOCKET * sd, const char *domain)
{
    int retval;
    char rbuf[MINBUF] = { 0 };

    if (!quiet) {
        printf("\rCommunicating with SMTP server...");
        fflush(stdout);
    }

    /**
     * We will be calling this function after ehlo() has already
     * been called.  Since ehlo() already grabs the header, go
     * straight into sending the HELO
    **/
    if (writeresponse(sd, "HELO %s\r\n", domain) < 0) {
        set_smtp_error("Lost connection to SMTP server");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- HELO\n");
    fflush(stdout);
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 250)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s\n", rbuf);
    fflush(stdout);
#endif

    return (retval);
}


static int
ehlo(SOCKET * sd, const char *domain)
{
    int retval;
    char rbuf[MINBUF] = { 0 };

    if (!quiet) {
        printf("\rCommunicating with SMTP server...");
        fflush(stdout);
    }

    /* This initiates the connection, so let's read the header first */
    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 220)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n--> %s", rbuf);
    fflush(stdout);
#endif

    if (writeresponse(sd, "EHLO %s\r\n", domain) < 0) {
        set_smtp_error("Lost connection to SMTP server");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n<-- EHLO %s\r\n", domain);
    fflush(stdout);
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 250)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n--> %s", rbuf);
    fflush(stdout);
#endif

    return (retval);
}

/** 
 * Send the MAIL FROM: command to the smtp server 
**/

static int
mailfrom(SOCKET * sd, const char *email)
{
    int retval = 0;
    char rbuf[MINBUF] = { 0 };

    /* Create the MAIL FROM: command */
    if (writeresponse(sd, "MAIL FROM:<%s>\r\n", email) < 0) {
        set_smtp_error("Lost connection with SMTP server");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n<-- MAIL FROM:<%s>\r\n", email);
#endif

    /* read return message and let's return it's code */
    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 250)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n--> %s", rbuf);
#endif

    return (retval);
}

/**
 * Send the RCPT TO: command to the smtp server
**/

static int
rcpt(SOCKET * sd, char *email)
{
    int retval = 0;
    char rbuf[MINBUF] = { 0 };

    /* Create the RCPT TO: command    and send it */
    if (writeresponse(sd, "RCPT TO:<%s>\r\n", email) < 0) {
        set_smtp_error("Lost connection with SMTP server");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n<-- RCPT TO:<%s>\r\n", email);
    fflush(stdout);
#endif

    /* Read return message and let's return it's code */
    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || ((retval != 250) && (retval != 251))) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n--> %s", rbuf);
    fflush(stdout);
#endif

    return (retval);
}

/** 
 * Send the DATA command to the smtp server ( no data, just the command )
**/

static int
data(SOCKET * sd)
{
    int retval = 0;
    char rbuf[MINBUF] = { 0 };

    if (!quiet) {
        printf("\rSending DATA...                    ");
        fflush(stdout);
    }

    /* Create the DATA command and send it */
    if (writeresponse(sd, "DATA\r\n") < 0) {
        set_smtp_error("Lost connection with SMTP server");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("\r\n<-- DATA\r\n");
#endif

    /* Read return message and let's return it's code */
    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 354)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s", rbuf);
#endif

    return (retval);
}

/**
 * Send the data from a open file to the smtp server 
**/

static int
send_data(SOCKET * sd, FILE * message)
{
    int retval = 0;
    char rbuf[100] = { 0 };
    char sbuf[100] = { 0 };
    struct prbar *pb;

    assert(message != NULL);
    assert(sd != NULL);

    /* start up our progress bar */
    pb = prbar_init(message);

    /* Let's go in a loop to get the data from the file and spit it out to sock */
    while (!feof(message) && !ferror(message)) {
        int count;

        count = fread(rbuf, 1, sizeof(rbuf) - 1, message);
        if (count > 0) {
            if (ssend(rbuf, count, sd) < 0) {
                set_smtp_error(socket_get_error(sd));
                return (ERROR);
            }

            if (!quiet && pb != NULL)
                prbar_print(count, pb);
            memset(rbuf, 0, count);
        }

    }

    if (ferror(message)) {
        set_smtp_error("Error reading email file. Can't continue sending email");
        return (ERROR);
    }

    if (writeresponse(sd, "\r\n.\r\n") < 0) {
        set_smtp_error("Lost Connection with SMTP server: Send_data()");
        return (ERROR);
    }

    retval = readresponse(sd, sbuf, sizeof(sbuf) - 1);
    if ((retval == ERROR) || (retval != 250)) {
        if (retval != ERROR)
            set_smtp_error(sbuf);

        return (ERROR);
    }

    prbar_destroy(pb);
    return (retval);
}

/**
 * Send the QUIT command
**/

static int
quit(SOCKET * sd)
{
    int retval = 0;
    char rbuf[MINBUF] = { 0 };

    /* Create QUIT command and send it */
    if (writeresponse(sd, "QUIT\r\n") < 0) {
        set_smtp_error("Lost Connection with SMTP server: Quit()");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- QUIT\r\n");
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 221)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s", rbuf);
#endif

    return (retval);
}

/**
 * Send the RSET command. 
**/

static int
rset(SOCKET * sd)
{
    int retval = 0;
    char rbuf[MINBUF] = { 0 };

    /* Send the RSET command */
    if (writeresponse(sd, "RSET\r\n") < 0) {
        set_smtp_error("Socket write error: rset");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- RSET\n");
    fflush(stdout);
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 250)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }


#ifdef DEBUG_SMTP
    printf("--> %s\n", rbuf);
    fflush(stdout);
#endif

    return (retval);
}


/** 
 * SMTP AUTH login.
**/

static int
smtp_auth_login(SOCKET * sd, const char *user, const char *pass)
{
    int retval = 0;
    char data[MINBUF] = { 0 };
    char rbuf[MINBUF] = { 0 };

    mime_b64_encode_string(user, strlen(user), data, sizeof(data));
    if (writeresponse(sd, "AUTH LOGIN %s\r\n", data) < 0) {
        set_smtp_error("Socket write error: smtp_auth_login");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- AUTH LOGIN\n");
    fflush(stdout);
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 334)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s\n", rbuf);
    fflush(stdout);
#endif

    /* Encode the password */
    memset(data, '\0', sizeof(data));
    mime_b64_encode_string(pass, strlen(pass), data, sizeof(data));
    if (writeresponse(sd, "%s\r\n", data) < 0) {
        set_smtp_error("Socket write error: smtp_auth_login");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- %s\n", data);
    fflush(stdout);
#endif

    /* Read back "OK" from server */
    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 235)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s\n", rbuf);
    fflush(stdout);
#endif

    return (0);
}

static int
smtp_auth_plain(SOCKET * sd, const char *user, const char *pass)
{
    int retval = 0;
    int uplen = 0;
    char *up;
    char data[MINBUF] = { 0 };
    char rbuf[MINBUF] = { 0 };

    if (writeresponse(sd, "AUTH PLAIN\r\n") < 0) {
        set_smtp_error("Socket write error: smtp_auth_plain");
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- AUTH PLAIN\n");
    fflush(stdout);
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 334)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s\n", rbuf);
    fflush(stdout);
#endif

    uplen = strlen(user) + strlen(pass) + 3;
    up = xmalloc(uplen);
    snprintf(up, uplen, "%c%s%c%s", '\0', user, '\0', pass);
    mime_b64_encode_string(up, uplen, data, sizeof(data));

    if (writeresponse(sd, "%s\r\n", data) < 0) {
        set_smtp_error("Socket write error: smtp_auth_plain");
        free(up);
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("<-- %s\n", data);
    fflush(stdout);
#endif

    retval = readresponse(sd, rbuf, sizeof(rbuf) - 1);
    if ((retval == ERROR) || (retval != 235)) {
        if (retval != ERROR)
            set_smtp_error(rbuf);

        free(up);
        return (ERROR);
    }

#ifdef DEBUG_SMTP
    printf("--> %s\n", rbuf);
    fflush(stdout);
#endif


    free(up);
    return (0);
}
