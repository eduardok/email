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
#include <errno.h>

#include <sys/wait.h>
#include <sys/stat.h>

#include "email.h"
#include "utils.h"
#include "execgpg.h"
#include "error.h"

/**
 * Calls gpg with popen so that we may write to stdin 
 * to pass gpg the password provided.  
**/

static int
execgpg(const char *command, char *passwd)
{
    FILE *gpg_stdin = NULL;

    if (!passwd)
        passwd = getpass("Please enter your GPG password: ");

    gpg_stdin = popen(command, "w");
    if (!gpg_stdin)
        return (-1);

    /* Push password to stdin */
    fputs(passwd, gpg_stdin);
    pclose(gpg_stdin);

    return (0);
}

/**
 * Calls gpg to sign the message 
 * returns an open FILE 
**/

FILE *
call_gpg_sig(const char *infile, const char *outfile)
{
    int retval;
    size_t size;
    FILE *retfile;
    char *command = NULL;
    char *gpg_bin, *gpg_pass, *email_addr;
    char gpg_path[MINBUF] = { 0 };

    assert(infile != NULL);
    assert(outfile != NULL);

    gpg_bin = get_conf_value("GPG_BIN");
    gpg_pass = get_conf_value("GPG_PASS");
    email_addr = get_conf_value("MY_EMAIL");
    if (!gpg_bin) {
        fatal("You must specify the path to GPG in email.conf\n");
        return (NULL);
    }

    expand_path(gpg_path, gpg_bin, sizeof(gpg_path));
    size = strlen(gpg_path) + MINBUF;

    command = xmalloc(size);
    snprintf(command, size, "%s --armor -o '%s' --no-secmem-warning --passphrase-fd 0"
             " --no-tty --digest-algo=SHA1 --sign --detach -u '%s' '%s'",
             gpg_path, outfile, email_addr, infile);

    /* Call gpg with our wrapper function */
    retval = execgpg(command, gpg_pass);
    free(command);

    if (retval == -1) {
        fatal("Error executing: %s", gpg_path);
        return (NULL);
    }

    retfile = fopen(outfile, "r+");
    return (retfile);
}


/**
 * Calls gpg to sign and encrypt message
 * returns and open FILE
**/

FILE *
call_gpg_encsig(const char *infile, const char *outfile)
{
    int retval;
    size_t size;
    FILE *retfile;
    char *encto, *command;
    char *gpg_bin, *gpg_pass;
    char gpg_path[MINBUF] = { 0 };

    assert(infile != NULL);
    assert(outfile != NULL);

    gpg_bin = get_conf_value("GPG_BIN");
    gpg_pass = get_conf_value("GPG_PASS");
    if (!gpg_bin) {
        fatal("You must specify the path to GPG in email.conf\n");
        return (NULL);
    }

    /* Get the first email from Mopts.to */
    encto = getfirstemail();
    size = strlen(gpg_path) + MINBUF;

    expand_path(gpg_path, gpg_bin, sizeof(gpg_path) - 1);
    command = xmalloc(size);
    snprintf(command, size,
             "%s -a -o '%s' --passphrase-fd 0 --no-tty -r '%s' -s -e '%s'",
             gpg_path, outfile, encto, infile);

    retval = execgpg(command, gpg_pass);

    free(encto);
    free(command);
    if (retval == -1) {
        fatal("Error executing: %s", gpg_path);
        return (NULL);
    }

    retfile = fopen(outfile, "r+");
    return (retfile);
}

/**
 * Calls gpg with execlp in a child fork().
 * returns an open encrypted FILE
**/

FILE *
call_gpg_enc(const char *infile, const char *outfile)
{
    int status;
    int retval, execval;
    FILE *retfile;
    char *encto, *gpg_bin;
    char gpg_path[MINBUF] = { 0 };

    assert(infile != NULL);
    assert(outfile != NULL);

    gpg_bin = get_conf_value("GPG_BIN");
    if (!gpg_bin) {
        fatal("You must specify the path to GPG in email.conf\n");
        return (NULL);
    }

    /* Get the first email from Mopts.to */
    encto = getfirstemail();
    expand_path(gpg_path, gpg_bin, sizeof(gpg_path) - 1);

    /* Fork and call gpg and store error code in shared memory */
    retval = fork();
    if (retval == 0) {          /* Child process */
        execval = execlp(gpg_path, "gpg", "--no-tty", "-a", "-o",
                         outfile, "-r", encto, "-e", infile, NULL);

        /* If we get here, there was an issue! */
        fatal("Error executing: %s", gpg_path);
        _exit(execval);
    }
    else if (retval > 0) {      /* Parent process */
        while (waitpid(retval, &status, 0) < 0);        /* Empty loop */
    }
    else {                      /* Error happened */

        fatal("Could not fork()");
        return (NULL);
    }

    /* Check if child exited with a code. */
    if (WIFEXITED(status) != 0) {
        if (WEXITSTATUS(status) != 0)
            return (NULL);
    }

    retfile = fopen(outfile, "r+");
    return (retfile);
}
