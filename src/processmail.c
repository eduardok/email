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
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "email.h"
#include "ipfunc.h"
#include "utils.h"
#include "smtpcommands.h"
#include "processmail.h"
#include "progress_bar.h"
#include "error.h"


/**
 * will invoke the path specified to sendmail with any 
 * options specified and it will send the mail via sendmail...
**/

int
process_internal(char *sm_bin, FILE * message)
{
    int bytes = 0;
    char buf[MINBUF];
    struct prbar *bar;
    FILE *open_sendmail;
    char path_to_sendmail[MINBUF];

    bar = prbar_init(message);
    expand_path(path_to_sendmail, sm_bin, sizeof(path_to_sendmail));

    open_sendmail = popen(path_to_sendmail, "w");
    if (!open_sendmail) {
        fatal("Could not open internal sendmail path: %s", path_to_sendmail);
        return (ERROR);
    }

    /* Make sure 'message' is at the top of the file */
    fseek(message, SEEK_SET, 0);

    /* Loop through getting what's out of message and sending it to sendmail */
    while (fgets(buf, sizeof(buf), message) != NULL) {
        bytes = strlen(buf);
        fputs(buf, open_sendmail);
        if (!quiet && bar != NULL)
            prbar_print(bytes, bar);
    }

    if (ferror(message))
        return (ERROR);

    fflush(open_sendmail);
    fclose(open_sendmail);

    prbar_destroy(bar);
    return (TRUE);
}

/**
 * Prints a user friendly error message when we encounter and 
 * error with SMTP communications.
**/

static void
print_smtp_error()
{
    fprintf(stderr, "\n");
    fatal("Smtp error: %s\n", smtp_errstring());
}

/**
 * Gets the users smtp password from the configuration file.
 * if it does not exist, it will prompt them for it.
**/

static char *
get_smtp_pass(void)
{
    char *retval;

    retval = get_conf_value("SMTP_AUTH_PASS");
    if (!retval)
        retval = getpass("Enter your SMTP Password: ");

    return (retval);
}

/**
 * This function will take the FILE and process it via a
 * Remote SMTP server...
**/
int
process_remote(const char *smtp_serv, int smtp_port, FILE * final_email)
{
    SOCKET *sd;
    int retval;
    char *smtp_auth = NULL;
    char *email_addr = NULL;
    char *user = NULL;
    char *pass = NULL;
    char nodename[MINBUF] = { 0 };

    email_addr = get_conf_value("MY_EMAIL");
    if (gethostname(nodename, sizeof(nodename) - 1) < 0)
        snprintf(nodename, sizeof(nodename) - 1, "geek");

    /* Get other possible configuration values */
    smtp_auth = get_conf_value("SMTP_AUTH");
    if (smtp_auth) {
        user = get_conf_value("SMTP_AUTH_USER");
        if (!user) {
            fatal("You must set SMTP_AUTH_USER in order to user SMTP_AUTH\n");
            return (ERROR);
        }

        pass = get_smtp_pass();
        if (!pass) {
            fatal("Failed to get SMTP Password.\n");
            return (ERROR);
        }
    }

    sd = socket_connect(smtp_serv, smtp_port);
    if (sd == NULL) {
        fatal("Could not connect to server: %s on port: %d", smtp_serv, smtp_port);
        return (ERROR);
    }

    /* Start SMTP Communications */
    if (init_smtp(sd, nodename) == ERROR) {
        print_smtp_error();
        return (ERROR);
    }

    /* See if we're using SMTP_AUTH. */
    if (smtp_auth) {
        retval = init_smtp_auth(sd, smtp_auth, user, pass);
        if (retval == ERROR) {
            print_smtp_error();
            return (ERROR);
        }
    }

    retval = set_smtp_mail_from(sd, email_addr);
    if (retval == ERROR) {
        print_smtp_error();
        return (ERROR);
    }

    retval = set_smtp_recipients(sd, Mopts.to, Mopts.cc, Mopts.bcc);
    if (retval == ERROR) {
        print_smtp_error();
        return (ERROR);
    }

    retval = send_smtp_data(sd, final_email);
    if (retval == ERROR) {
        print_smtp_error();
        return (ERROR);
    }

    socket_close(sd);
    return (TRUE);
}
