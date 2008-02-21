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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>

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
#include "hash.h"
#include "error.h"

static char *xtmpname(char *, size_t);

/**
 * is_blank is a direct inturpritation of the gnu isblank 
 * function. will return true if blank
**/

int
is_blank(int stat)
{
    if (stat == ' ' || stat == '\t')
        return (TRUE);
    else
        return (FALSE);
}

/**
 * will take string2 and move it into string1, but will not 
 * exceed the size past to it, and will only concat if string1 
 * has enough room.
**/

int
safeconcat(char *to, const char *from, size_t size)
{
    size_t size_left = 0;
    size_t lento = strlen(to);
    size_t lenfr = strlen(from);

    if (lento > size)
        size_left = lento - size - 1;
    else
        size_left = size - lento - 1;

    memcpy(to + lento, from, size_left);
    if (size_left < lenfr)
        return (size_left);

    return (lenfr);
}

/**
 * takes a string that is a supposed file path, and 
 * checks for certian wildcards ( like ~ for home directory ) 
 * and resolves to an actual absolute path.
**/

void
expand_path(char *store, char *path, size_t size)
{
    char *home_dir = NULL;
    struct passwd *pw = NULL;

/* first, let's check that we can expand the path */
    if ((strlen(path) == 0) || ((path[0] != '~') && (path[0] != '&'))) {
        snprintf(store, size, "%s", path);
        return;
    }

  /** If path is equal to &, we want to expand to EMAIL HOME **/
    if (path[0] == '&') {
        if (strlen(path) > 1) {
            path++;
            if (*path != '/')
                *path = '/';
            snprintf(store, size, "%s%s", EMAIL_DIR, path);
        }
        else
            snprintf(store, size, "%s", EMAIL_DIR);

        return;
    }

    /*
     * if strlen of path is 1 or the next char in path is a '/'
     * we are talking about the current user of the system
     */

    if ((strlen(path) == 1) || (path[1] == '/')) {
        if (strlen(path) > 1)
            path++;             /* get past tilde */
        else
            *path = '/';        /* else, set the tilde to a slash */

        pw = getpwuid(getuid());
        if (pw)
            home_dir = pw->pw_dir;
    }
    else {
        char name_buf[100] = { 0 };
        char *name_ptr = name_buf;

        path++;                 /* get past tilde mark */

        /* get the name up to a '/' or end of line */
        while ((*path != '/') && (*path != '\0'))
            *name_ptr++ = *path++;


        pw = getpwnam(name_buf);
        if (pw)
            home_dir = pw->pw_dir;
    }

    /* If we failed to find anything, return path given */
    if (!home_dir) {
        snprintf(store, size, "%s", path);
        return;
    }

    snprintf(store, size, "%s%s", home_dir, path);
}

/**
 * Removes the \r and \n at the end of the line provided.
 * This gets rid of standards UNIX \n line endings and 
 * also DOS CRLF or \r\n line endings.
**/

void
chomp(char *str)
{
    char *cp;

    if (str && (cp = strrchr(str, '\n')))
        *cp = '\0';
    if (str && (cp = strrchr(str, '\r')))
        *cp = '\0';
}

/**
 * xrealloc is a wrapper for realloc()
**/

void *
xrealloc(void *ptr, size_t size)
{
    void *retptr = NULL;

    retptr = realloc(ptr, size);
    if (retptr == NULL) {
        fatal("Could not Realloc");
        proper_exit(ERROR);
    }

    return (retptr);
}

/**
 * xmalloc is obviously just a wrapper for malloc().  
 * Just some error checking here so that we don't have 
 * to clutter our beautiful code with calls to malloc 
 * and error check all the time.
**/

void *
xmalloc(size_t size)
{
    void *retptr = NULL;

    retptr = malloc(size);
    if (retptr == NULL) {
        fatal("Could not Malloc");
        proper_exit(ERROR);
    }

    memset(retptr, 0, size);
    return (retptr);
}

/**
 * xcalloc is the same as xmalloc above.
**/

void *
xcalloc(size_t nmemb, size_t size)
{
    void *retptr = NULL;

    retptr = calloc(nmemb, size);
    if (!retptr) {
        fatal("Could not Calloc");
        proper_exit(ERROR);
    }

    return (retptr);
}


/**
 * A wrapper for strdup so that we can check for errors
 * basically, exactly like the above functions.
**/

char *
xstrdup(const char *str)
{
    char *ret;
    size_t size = strlen(str) + 1;

    ret = malloc(size);
    if (ret == NULL) {
        fatal("xstrdup() Could not malloc()");
        proper_exit(ERROR);
    }

    memcpy(ret, str, size);
    return (ret);
}

int
copyfile(const char *from, const char *to)
{
    FILE *ffrom, *fto;
    char buf[MINBUF] = { 0 };

    ffrom = fopen(from, "r");
    fto = fopen(to, "w");
    if (!ffrom || !fto)
        return (-1);

    while (fread(buf, sizeof(char), sizeof(buf) - 1, ffrom) > 0) {
        fwrite(buf, sizeof(char), strlen(buf), fto);
        memset(buf, '\0', sizeof(buf));
    }

    if (ferror(ffrom) || ferror(fto))
        return (-1);

    fclose(ffrom);
    fclose(fto);
    return (0);
}

/**
 * checks to see if the TEMP_FILE is around... if it is
 * it will move it to the users home directory as dead.letter.
**/

void
dead_letter(void)
{
    list_t tmpfile;
    char dead_letter[MINBUF] = { 0 };

    /* 
     * Since the very first temp file made
     * is the file that gets the original input
     * we'll just pop the list of tmp files
     */

    tmpfile = list_gettop(tmpfiles);
    if (!tmpfile || access(tmpfile->a_data, F_OK) < 0) {
        return;
    }

    expand_path(dead_letter, "~/dead.letter", sizeof(dead_letter) - 1);
    if (copyfile(tmpfile->a_data, dead_letter) < 0)
        warning("Could not save dead letter to %s", dead_letter);
}



/**
 * Gererate a string of random characters
**/

static const char letters[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz" 
                              "0123456789";

#define SIZEOF_LETTERS 62

void
random_string(char *buf, size_t size)
{
    size_t i;
    long randval;
    struct timeval mill;

    gettimeofday(&mill, NULL);
    srand((getuid() + getpid()) + (mill.tv_usec << 16));

    for (i = 0; i < size - 1; i++) {
        randval = rand() / (RAND_MAX / SIZEOF_LETTERS);
        *buf++ = letters[randval];
    }
}

/**
 * Get the first element from the Mopts.to list of emails
 * and return it without the name or formating. just the
 * email address itself.
**/

char *
getfirstemail(void)
{
    int len;
    char *retval, *tmp;
    list_t email = NULL;

    /* Should always pop something, or we have a bug */
    email = list_gettop(Mopts.to);
    assert(email != NULL);

    /**
     * we will get to the first occurance of <.
     * this brings us to the start of the formated
     * email address.
    **/

    /* If we haven't found a <, consider the e-mail unformatted. */
    tmp = strchr(email->b_data, '<');
    if (!tmp) {
        tmp = email->b_data;
    } else {
        /* strchr only brings us to the '<', Get past it */
        ++tmp;
    }

    len = strlen(tmp);
    retval = xmalloc(len);
    memcpy(retval, tmp, len);

    /* get rid of '>' if it exists */
    tmp = strchr(retval, '>');
    if (tmp) {
        *tmp = '\0';
    }

    return (retval);
}

static char *
xtmpname(char *buf, size_t size)
{
    size_t len;
    char *dir;
    char tmpbuf[10] = { 0 };
    char *tmpdir = NULL;

    assert(buf != NULL);
    tmpdir = get_conf_value("TEMP_DIR");

    /* Must at least have a size of 16 or greater */
    if (size < 16) {
        errno = ENOMEM;
        return (NULL);
    }

    /* Find what directory to use */
    if (tmpdir)
        dir = tmpdir;
    else {
        dir = getenv("TMPDIR");
        if (!dir)
            dir = "/tmp";
    }

    for (;;) {
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        random_string(tmpbuf, sizeof(tmpbuf));

        /**
         * If the dir name is too big, default to /tmp 
         * size - 11 because we need to leave room for the name 
        **/

        len = strlen(dir);
        if (len >= size - 11)
            dir = "/tmp";

        snprintf(buf, size, "%s/.EM%s", dir, tmpbuf);

        /* If file doesn't already exist */
        if (access(buf, F_OK) < 0)
            break;

    }

    return (buf);
}

/**
 * Creates a tmpfile and places it's name in a linked list
 * so that it may be removed properly later.
**/

char *
create_tmpname(char *buf, size_t size)
{
    /* list_t tmpfiles; in email.h */

    xtmpname(buf, size);
    list_insert(&tmpfiles, buf, NULL);

    return (buf);
}

/**
 * Destroy temp files and remove each one destroyed
 * from the linked list
**/

void
destroy_tmpfiles(list_t files)
{
    list_t filename;

    while ((filename = list_gettop(files))) {
        remove(filename->a_data);
        list_pop(&files);
    }
}

/**
 * Simply a wrapper function for setting a value and key
 * in the hash table.
**/

void
set_conf_value(const char *token, const char *value)
{
    /* table_t table in email.h */

    hash_insert(&table, token, value);
}

/**
 * Simply a wrapper function for getting a value out of
 * the hash table.
**/

char *
get_conf_value(const char *token)
{
    /* table_t table in email.h */

    return ((char *) hash_lookup_key(table, token));
}

/**
 * Exit just handles all the signals and exiting of the 
 * program by freeing the allocated memory  and writing 
 * the dead.letter if we had a sudden interrupt...
**/

void
proper_exit(int sig)
{
    switch (sig) {
        case SIGINT:
            if (!quiet)
                fprintf(stderr, "\nCaught SIGINT, Shutting down...   \n");
            dead_letter();
            break;

        case SIGHUP:
            if (!quiet)
                fprintf(stderr, "\nCaught SIGHUP, Shutting down...   \n");
            dead_letter();
            break;

        case SIGTERM:
            if (!quiet)
                fprintf(stderr, "\nCaught SIGTERM, Shutting down...  \n");
            dead_letter();
            break;

        case SIGPIPE:
            fprintf(stderr, "\nLost communications with SMTP server...\n");
            dead_letter();
            break;

        case ERROR:
            fprintf(stderr, "\nCaught an ERROR, Shutting down...  \n");
            dead_letter();
            break;

        case EASY:
            if (!quiet)
                fprintf(stderr, "\nExiting email normally...       \n");
            dead_letter();
            break;

        default:
            if (!quiet)
                fprintf(stderr, "\nE-Mail Sent \n");
            sig = 0;
            break;
    }


    /* removes files and kills linked list */
    destroy_tmpfiles(tmpfiles);

    /* Free lists */
    if (Mopts.attach) {
        list_free(Mopts.attach);
    }
    if (Mopts.headers) {
        list_free(Mopts.headers);
    }
    if (Mopts.to) {
        list_free(Mopts.to);
    }
    if (Mopts.cc) {
        list_free(Mopts.cc);
    }
    if (Mopts.bcc) {
        list_free(Mopts.bcc);
    }

    hash_destroy(table);
    exit(sig);
}

int
explode_to_list(char *str, const char *del, list_t *ret)
{
    int del_items = 0;
    char *next = strtok(str, del);
    while (next) {
        list_insert(ret, next, NULL);
        next = strtok(NULL, del);
        del_items++;
    }
    return del_items;
}
