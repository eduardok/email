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

#include <pwd.h>

#include <sys/utsname.h>
#include <sys/types.h>

#include "email.h"
#include "conf.h"
#include "utils.h"
#include "dstring.h"
#include "error.h"

#define MAX_CONF_VARS 20

/* There are the variables accepted in the configuration file */
static char conf_vars[MAX_CONF_VARS][MINBUF] = {
    "SMTP_SERVER",
    "SMTP_PORT",
    "SENDMAIL_BIN",
    "MY_NAME",
    "MY_EMAIL",
    "REPLY_TO",
    "SIGNATURE_FILE",
    "SIGNATURE_DIVIDER",
    "ADDRESS_BOOK",
    "SAVE_SENT_MAIL",
    "TEMP_DIR",
    "GPG_BIN",
    "GPG_PASS",
    "SMTP_AUTH",
    "SMTP_AUTH_USER",
    "SMTP_AUTH_PASS"
};

/* Function declaration */
static int hashit(char *, char *);
static int read_config(FILE *);
static char *get_system_name(void);
static char *get_system_email(void);
static int check_var(dstring);

/**
 * This function is used to just run through the configuration file
 * and simply check the syntax.  No extra configuration takes place
 * here.
**/

void
check_config(void)
{
    int line = 0;
    FILE *config;
    char home_config[MINBUF] = { 0 };

    if (conf_file == NULL) {
        expand_path(home_config, "~/.email.conf", sizeof(home_config));
        config = fopen(home_config, "r");
        if (!config)
            config = fopen(MAIN_CONFIG, "r");
    }
    else
        config = fopen(conf_file, "r");

    /* Couldn't open any possible configuration file */
    if (!config) {
        fatal("Could not open any possible configuration file");
        proper_exit(ERROR);
    }

    line = read_config(config);
    fclose(config);
    if (line > 0) {
        fatal("Line: %d of email.conf is improperly formatted.\n", line);
        proper_exit(ERROR);
    }
}



/**
 * this function will read the configuration file and store all values
 * in a hash table.  If some values were specified on the comand line
 * it will allow those to override what is in the configuration file.
 * If any of the manditory configuration variables are not found on the
 * command line or in the config file, it will set default values for them.
**/

void
configure(void)
{
    int line = 0;
    FILE *config;
    char home_config[MINBUF] = { 0 };

    /* try to open the different possible configuration files */
    if (conf_file == NULL) {
        expand_path(home_config, "~/.email.conf", sizeof(home_config));
        config = fopen(home_config, "r");
        if (!config)
            config = fopen(MAIN_CONFIG, "r");
    }
    else
        config = fopen(conf_file, "r");

    /**
     * smtp server can be overwitten by command line option -r
     * in which we don't want to insist on a configuration file
     * being available.  If there isn't a config file, we will 
     * and an SMTP server was specified, we will set smtp_port
     * to 25 as a default if they didn't already specify it
     * on the command line as well.  If they didn't specify an
     * smtp server, we'll default to sendmail.
    **/

    if (!config) {
        if (!get_conf_value("MY_NAME"))
            set_conf_value("MY_NAME", get_system_name());

        if (!get_conf_value("MY_EMAIL"))
            set_conf_value("MY_EMAIL", get_system_email());


        /* If they didn't specify an smtp server, use sendmail */
        if (!get_conf_value("SMTP_SERVER"))
            set_conf_value("SENDMAIL_BIN", "/usr/lib/sendmail -t -i");
        else {
            if (!get_conf_value("SMTP_PORT"))
                set_conf_value("SMTP_PORT", "25");
        }

    }
    else {
        line = read_config(config);
        if (line > 0) {
            fatal("Line number %d is incorrectly formated in email.conf\n", line);
            proper_exit(ERROR);
        }

        /* If port wasn't in config file or specified on command line */
        if (!get_conf_value("SMTP_PORT"))
            set_conf_value("SMTP_PORT", "25");

        /* If name wasn't specified */
        if (!get_conf_value("MY_NAME"))
            set_conf_value("MY_NAME", get_system_name());

        /* If email address wasn't in the config file */
        if (!get_conf_value("MY_EMAIL"))
            set_conf_value("MY_EMAIL", get_system_email());
    }

    if (config)
        fclose(config);
}

/**
 * Make sure that var is part of a possible 
 * configuration variable in the configure file
**/

static int
check_var(dstring var)
{
    int i;

    const char *the_var = dstring_getString(var);
    for (i = 0; i < MAX_CONF_VARS; i++) {
        if (strcasecmp(the_var, conf_vars[i]) == 0)
            return (0);
    }

    /* If we came out of the loop, the variable is invalid */
    return (-1);
}

/**
 * Check the hash values to make sure they aren't 
 * NULL.  IF they aren't, then Hash them, other
 * wise, ERROR 
**/

static int
hashit(char *var, char *val)
{
    if ((*var == '\0') && (*val == '\0'))       /* Nothing to hash */
        return (-1);
    if ((*var == '\0') || (*val == '\0'))       /* Something went wrong */
        return (ERROR);

    set_conf_value(var, val);
    return (0);
}


/**
 * Read the configuration file char by char and make
 * sure each char is taken care of properly.
 * Hash each token we get.  Newlines are considered the
 * end of the expression if a \ is not found.
**/

int
read_config(FILE * in)
{
    int ch, line = 1;
    int retval = 0;
    dstring var, val;
    dstring ptr;   /* start with var first */

    var = dstring_getNew(100);
    val = dstring_getNew(100);
    ptr = var;

    while ((ch = fgetc(in)) != EOF) {
        switch (ch) {
            case '#':
                while ((ch = fgetc(in)) != '\n');       /* Deal with newline below */
                break;

            case '\\':
                ch = fgetc(in); /* check next char */
                if (ch == '\r') {
                    ch = fgetc(in);     /* Looking for a \n */
                }
                if (ch != '\n') {
                    dstring_copyChar(ptr, ch);
                }
                line++;
                ch = 0;         /* Clear character incase of newline */
                break;

            case '\'':
                if (dstring_copyUpTo(ptr, ch, in) == '\n') {
                    ch = line;      /* If newline was found before end quote */
                    goto exit;
                }
                break;

            case '"':
                if (dstring_copyUpTo(ptr, ch, in) == '\n') {
                    ch = line;      /* If newline was found before end quote */
                    goto exit;
                }
                break;

            case '=':
                if (dstring_getLength(val) != 0) {
                    ch = line;
                    goto exit;
                } else if (check_var(var) < 0) {
                    fatal("Variable: '%s' is not valid\n", var);
                    ch = line;
                    goto exit;
                }
                ptr = val;
                break;

            case ' ':
                /* Nothing for spaces */
                break;

            case '\t':
                /* Nothing for tabs */
                break;

            case '\r':
                /* Ignore */
                break;

            case '\n':
                /* Handle Newlines below */
                break;

            default:
                dstring_copyChar(ptr, ch);
                break;
        }

        /* See about the newline, hash vals if possible */
        if (ch == '\n') {
            line++;
            char *the_var = dstring_getString(var);
            char *the_val = dstring_getString(val);
            retval = hashit(the_var, the_val);
            if (retval == -1) {
                continue;       /* No error, just keep going */
            } else if (retval == ERROR) {
                ch = line;
                goto exit;
            }

            /* Everything went fine. */
            dstring_delete(var);
            dstring_delete(val);
            var = dstring_getNew(100);
            val = dstring_getNew(100);
            ptr = var;
        }
    }

exit:
    dstring_delete(var);
    dstring_delete(val);
    return (ch);
}

/**
 * Get the user name of the person running me
 * Return Value: 
 *    malloced string 
**/

static char *
get_system_name(void)
{
    int uid = getuid();
    char *name = NULL;
    struct passwd *ent;

    ent = getpwuid(uid);
    if (!ent) {
        name = xstrdup("Unknown User");
        return (name);
    }

    /* Try to get the "Real Name" of the user. Else, the user name */
    if (ent->pw_gecos)
        name = xstrdup(ent->pw_gecos);
    else
        name = xstrdup(ent->pw_name);

    return (name);
}

/**
 * Get the email address of the person running me.
 * this is done by getting the "user name" of the current
 * person, and the host name running me.
 * Return value:
 *    malloced string
**/

static char *
get_system_email(void)
{
    int uid = getuid();
    char temp[100] = { 0 };
    char *email, *name, *host;
    struct utsname hinfo;
    struct passwd *ent;

    ent = getpwuid(uid);
    if (!ent)
        name = "unknown";
    else
        name = ent->pw_name;

    if (uname(&hinfo) < 0)
        host = "localhost";
    else
        host = hinfo.nodename;

    /* format it all */
    snprintf(temp, sizeof(temp), "%s@%s", name, host);
    email = xstrdup(temp);

    return (email);
}
