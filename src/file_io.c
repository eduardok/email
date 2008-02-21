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
#include "file_io.h"
#include "sig_file.h"
#include "error.h"

static int open_editor(const char *, const char *);
static int question(const char *);

/**
 * ReadInput: This function just reads in a file from STDIN which 
 * allows someone to redirect a file into email from the command line.
**/

int
read_input(const char *filename)
{
    FILE *out;
    char filepath[MINBUF] = { 0 };
    char buf[MINBUF] = { 0 };
    char *sig_file = NULL;

    assert(filename != NULL);

    sig_file = get_conf_value("SIGNATURE_FILE");
    if (!(out = fopen(filename, "w+"))) {
        fatal("Could not open file temporary file");
        return (ERROR);
    }

    while (fgets(buf, sizeof(buf), stdin) != NULL)
        fprintf(out, "%s", buf);

    if (ferror(stdin)) {
        fclose(out);
        return (ERROR);
    }

    /* If they specified a signature file, let's append it */
    if (sig_file) {
        expand_path(filepath, sig_file, MINBUF);

        /* Now that we know we have an absolute file path to the signature file... */
        append_sig(out, filepath);
    }

    fflush(out);
    fclose(out);
    return (TRUE);
}

/**
 * Question: will simply ask the question provided.  This 
 * function will only accept a yes or no (y or n even) answer
 * and loop until it gets that answer.  It will return
 * TRUE for 'yes', FALSE for 'no'
**/

static int
question(const char *question)
{
    char yorn[5] = { 0 };

    for (;;) {
        printf("%s[y/n]: ", question);
        fgets(yorn, sizeof(yorn), stdin);
        chomp(yorn);
        if ((strcasecmp(yorn, "no") == 0) || (strcasecmp(yorn, "n") == 0))
            return (FALSE);
        else if ((strcasecmp(yorn, "yes") == 0) || (strcasecmp(yorn, "y") == 0))
            break;
        else {
            printf("Invalid Response!\n");
            continue;
        }
    }

    return (TRUE);
}

/**
 * Will for and execute the program that is passed to it.
 * will return -1 if an error occurred.
**/

static int
open_editor(const char *editor, const char *filename)
{
    int retval, status;

    assert(filename != NULL);

    retval = fork();
    if (retval == 0) {          /* Child process */
        if (execlp(editor, editor, filename, NULL) < 0)
            exit(-1);
    }
    else if (retval > 0) {      /* Parent process */
        while (waitpid(retval, &status, 0) < 0);        /* Empty loop */
    }
    else                        /* Error occured */
        return (-1);

    if (WIFEXITED(status) != 0) {
        if (WEXITSTATUS(status) != 0)
            return (-1);
    }

    return (0);
}

/**
 * EditFile: this function basicly opens the editor of choice 
 * with a temp file and lets you create your message and send it.  
**/

int
edit_file(const char *filename)
{
    char *editor;
    char *sig_file = NULL;
    char absolute_filepath[MINBUF];
    FILE *out = NULL;
    struct stat sb;

    assert(filename != NULL);

    sig_file = get_conf_value("SIGNATURE_FILE");
    memset(&sb, 0, sizeof(sb));

    if (!(editor = getenv("EDITOR"))) {
        warning("Environment varaible EDITOR not set: Defaulting to \"vi\"\n");
        editor = "vi";
    }

    if (open_editor(editor, filename) < 0)
        warning("Error when trying to open editor '%s'", editor);

    /* if they quit their editor without makeing an email... */
    /* st_size will be 0 if stat failed and if file was not modified */
    stat(filename, &sb);
    if (sb.st_size <= 0) {
        if (question("You have an empty email, send anyway?") == FALSE)
            proper_exit(EASY);
    }

    /* If they specified a signature file, let's append it */
    if (sig_file) {
        if (!(out = fopen(filename, "a"))) {
            fatal("Can not open file just created");
            return (ERROR);
        }

        expand_path(absolute_filepath, sig_file, MINBUF);

        /* Now that we know we have an absoulte file path to the signature file... */
        append_sig(out, absolute_filepath);

        fflush(out);
        fclose(out);
    }

    return (TRUE);
}
