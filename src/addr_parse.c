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
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "email.h"
#include "utils.h"

static char *strip_email_name(char *);
static char *strip_email_addr(char *);
static int validate_email_string(char *);

/**
 * strip any quotes from the begining and end of the
 * email name passed.  
**/

static char *
strip_email_name(char *str)
{
    char *end_quote;

    if (!str)
        return (NULL);

    /* first remove end quote */
    if ((end_quote = strrchr(str, '"')))
        *end_quote = '\0';

    /* now remove begining quote */
    if ((*str == '"'))
        str++;

    return (str);
}

/**
 * strip any quotes and brackets from the begining of the 
 * email address that is passed.
**/

static char *
strip_email_addr(char *str)
{
    char *end_quote, *end_bracket;

    if (!str)
        return (NULL);

    /* first check if we have an end quote */
    if ((end_quote = strrchr(str, '"')))
        *end_quote = '\0';

    /* now check for end brace */
    if ((end_bracket = strrchr(str, '>')))
        *end_bracket = '\0';

    /* now check for begining quote and bracket */
    if ((*str == '"') || (*str == '<'))
        str++;
    if ((*str == '<') || (*str == '"'))
        str++;

    return (str);
}

/** 
 * just a wrapper for snprintf() to make code
 * more readable.  A reader will know that the email address is being
 * formated by reading this name.
**/

void
format_email_addr(char *name, char *address, char *buffer, size_t size)
{
    char *proper_name, *proper_addr;

    assert(address != NULL);

    proper_addr = strip_email_addr(address);
    if (name) {
        proper_name = strip_email_name(name);
        snprintf(buffer, size, "\"%s\" <%s>", proper_name, proper_addr);
    } else {
        snprintf(buffer, size, "<%s>", proper_addr);
    }
}

/**
 * will take one email address as an argument 
 * and make sure it has the appropriate syntax of an email address. 
 * if so, return TRUE, else return FALSE 
**/

int
validate_email(char *email_addy)
{
    char *copy;
    char *name, *domain;

    /* make a copy of email_addy so we don't ruin it with strtok */
    copy = xstrdup(email_addy);

    /* Get the name before the @ character. */
    name = strtok(copy, "@");
    if (name == NULL)
        goto BADEXIT;

    /* Get the domain after the @ character. */
    domain = strtok(NULL, "@");
    if (domain == NULL)
        goto BADEXIT;

    if (validate_email_string(name) == ERROR)
        goto BADEXIT;

    if (validate_email_string(domain) == ERROR)
        goto BADEXIT;

    free(copy);
    return (TRUE);

  BADEXIT:
    free(copy);
    return (ERROR);
}

/**
 * Validate_email_string is used within ValidateEmail to check the string passed
 * to make sure it only contains characters appropriate for email addresses.
 * It only allows alphanumerica characters and dots (.). Anything else is considered
 * an error and the function returns accordingly.This is RFC 822 standard address 
 * formating.
**/

static int
validate_email_string(char *str)
{
    if (!str)
        return (ERROR);

    /* RFC821 says name and domain can't be greater than 64 chars */
    if (strlen(str) > 64)
        return (ERROR);

    return (TRUE);
}

