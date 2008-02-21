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

#include "email.h"
#include "addr_parse.h"
#include "utils.h"
#include "addy_book.h"
#include "linked_lists.h"
#include "dstring.h"
#include "error.h"

/*
 * Entry is a structure in which we will hold a parsed
 * string from the address book.  
 * 
 * e_gors == "group" for group "single" for single and NULL for neither
 * e_name == Name of entry
 * e_addr == Addresses found with entry 
 *
 */

typedef struct {
    char *e_gors;
    char *e_name;
    char *e_addr;

} ENTRY;

/* Function declarations */

static list_t separate(char *);
static int make_entry(ENTRY *, char *, char *, char *);
static int get_entry(ENTRY *, list_t, FILE *);
static void insert_entry(list_t *, char *, char *);
static int store_group(list_t *, ENTRY *, FILE *);
static int add_entry(list_t *, ENTRY *, FILE *);
static int check_addr_book(list_t *, list_t, FILE *);
static void free_entry(ENTRY *);
static void check_and_copy(list_t *, list_t);


/** 
 * GetNames just calls the correct functions to 
 * split up the names passed to email and check
 * them against the address book and it will return 
 * a linked list 'list_t' with the correct separate names.
**/

list_t
getnames(char *string)
{
    FILE *book;
    char *addr_book;
    char bpath[MINBUF] = { 0 };
    list_t tmp = NULL;
    list_t ret_storage = NULL;

    addr_book = get_conf_value("ADDRESS_BOOK");

    /* Read in and hash the address book */
    if (!(tmp = separate(string))) {
        return NULL;
    }

    if (!addr_book)
        check_and_copy(&ret_storage, tmp);
    else {
        expand_path(bpath, addr_book, sizeof(bpath));
        book = fopen(bpath, "r");
        if (!book) {
            fatal("Can't open address book: '%s'\n", bpath);
            return (NULL);
        }

        if (check_addr_book(&ret_storage, tmp, book) == ERROR) {
            list_free(tmp);
            list_free(ret_storage);
            return (NULL);
        }

        fclose(book);
    }

#ifdef DEBUG_MEMORY
    {
        list_t current = NULL;
        list_t saved = NULL;

        current = list_getnext(ret_storage, &saved);
        while (current) {
            fprintf(stderr, "Mailing to: %s\n", current->b_data);
            current = list_getnext(NULL, &saved);
        }
    }
#endif

    list_free(tmp);
    return (ret_storage);
}

/** 
 * Seperate will take a string of command separated fields
 * and with separate them into a linked list for return 
 * by the function.
**/

static list_t
separate(char *string)
{
    char *next;
    int counter;
    list_t ret_storage = NULL;

    counter = 0;
    next = strtok(string, ",");

    while (next) {
        /* don't let our counter get over 99 address' */
        if (counter == 99)
            break;

      /**
       * make sure that next isn't too large, if it is, don't store it
       * get next element and move on...
      **/
        if (strlen(next) > MINBUF) {
            next = strtok(NULL, ",");
            continue;
        }

        /* Get rid of white spaces */
        while (is_blank(*next))
            next++;

        list_insert(&ret_storage, NULL, next);
        next = strtok(NULL, ",");
        counter++;
    }

    return ret_storage;
}

/**
 * Makes sure that all values are available for storage in
 * the ENTRY structure.  
 *
 * If NONE of the values are ready, it returns -1 to let 
 * the caller know that this is not an ERROR and should 
 * continue.
 *
 * If some of the values aren't available, something fishy
 * went on and we'll return ERROR
 *
 * Otherwise, store the values in the ENTRY struct.
**/

static int
make_entry(ENTRY * en, char *gors, char *name, char *addr)
{
    if ((*gors == '\0') && (*name == '\0') && (*addr == '\0'))
        return (-1);            /* No error */
    if ((*gors == '\0') || (*name == '\0') || (*addr == '\0'))
        return (ERROR);

    en->e_gors = xstrdup(gors);
    en->e_name = xstrdup(name);
    en->e_addr = xstrdup(addr);
    return (0);
}

/**
 * Parses the address book file in search of the correct
 * entry.  If the entry desired is found, then it is stored
 * in the ENTRY structure that is passed.
**/

static int
get_entry(ENTRY *en, list_t ent, FILE *book)
{
    int ch, line = 1;
    dstring ptr, gors;
    dstring name, addr;

    assert(book != NULL);
    rewind(book);               /* start at top of file */

    gors = dstring_getNew(100);
    name = dstring_getNew(100);
    addr = dstring_getNew(100);
    ptr = gors;
    en->e_gors = en->e_name = en->e_addr = NULL;

    while ((ch = fgetc(book)) != EOF) {
        switch (ch) {
            case '#':
                while ((ch = fgetc(book)) != '\n');     /* empty loop */
                break;

            case '\\':
                ch = fgetc(book);
                if (ch == '\r') {
                    ch = fgetc(book);
                }
                if (ch != '\n') {
                    dstring_copyChar(ptr, ch);
                }
                line++;
                ch = 0;
                break;

            case '\'':
                if (dstring_copyUpTo(ptr, ch, book) == '\n') {
                    ch = line;
                    goto exit;
                }
                break;

            case '"':
                if (dstring_copyUpTo(ptr, ch, book) == '\n') {
                    ch = line;
                    goto exit;
                }
                break;

            case ':':
                if (dstring_getLength(name) != 0) {
                    ch = line;
                    goto exit;
                }
                ptr = name;
                break;

            case '=':
                if (dstring_getLength(addr) != 0) {
                    ch = line;
                    goto exit;
                }
                ptr = addr;
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
                /* Handle newlines below */
                break;

            default:
                dstring_copyChar(ptr, ch);
                break;
        }

        if (ch == '\n') {
            char *the_name = dstring_getString(name);
            char *the_gors = dstring_getString(gors);
            char *the_addr = dstring_getString(addr);
            if (strcasecmp(the_name, ent->b_data) == 0) {
                if (make_entry(en, the_gors, the_name, the_addr) == ERROR) {
                    ch = line;
                    goto exit;
                }
                return (0);
            }

            /* Let's keep looking ! */
            dstring_delete(gors);
            dstring_delete(name);
            dstring_delete(addr);
            gors = dstring_getNew(100);
            name = dstring_getNew(100);
            addr = dstring_getNew(100);
            ptr = gors;
            line++;
        }
    }

exit:
    dstring_delete(gors);
    dstring_delete(name);
    dstring_delete(addr);

    /* Possible EOF or Line number */
    return (ch);
}

/**
 * Add an email to the list.  Make sure it is formated properly.
 * After formated properly, list_insert() it 
**/

static void
insert_entry(list_t * to, char *name, char *addr)
{
    if (validate_email(addr) == ERROR) {
        warning("Email address '%s' is invalid. Skipping...\n", addr);
        return;
    }
    list_insert(to, name, addr);
}

/**
 * Add an entry to the linked lists.
 * If entry is a group, we must separate the people inside of 
 * the group and re-call check_addr_book for those entries.
**/

static int
store_group(list_t * to, ENTRY * en, FILE * book)
{
    static int set_group;
    list_t tmp = NULL;

    if (set_group) {
        fatal("You can't define groups within groups!\n");
        return (ERROR);
    }

    tmp = separate(en->e_addr);
    set_group = 1;
    check_addr_book(to, tmp, book);
    set_group = 0;
    list_free(tmp);

    return (0);
}

/**
 * Wrapper for adding an entry.  Will call Store_group
 * or Store_single, or Store_email if the entry is 
 * one of the above.
**/

static int
add_entry(list_t * to, ENTRY * en, FILE * book)
{
    if (strcmp(en->e_gors, "group") == 0) {
        if (store_group(to, en, book) == ERROR)
            return (ERROR);
    } else {
        insert_entry(to, en->e_name, en->e_addr);
    }

    return 0;
}

/**
 * Loops through the linked list and calls Get_entry for
 * each name in the linked list.  It will add each entry into
 * the 'to' linked lists when an appropriate match is found
 * from the address book.
**/

static int
check_addr_book(list_t *to, list_t from, FILE *book)
{
    ENTRY en;
    int retval;
    list_t next = NULL;
    list_t saved = NULL;

    /* Go through list from, resolving to list curr */
    next = list_getnext(from, &saved);
    while (next) {
        retval = get_entry(&en, next, book);
        if (retval > 0) {
            fatal("Address book incorrectly formated on line %d\n", retval);
            return (ERROR);
        } else if (retval == EOF) {
            insert_entry(to, NULL, next->b_data);
        } else {
            if (add_entry(to, &en, book) == ERROR) {
                return ERROR;
            }
        }

        free_entry(&en);
        next = list_getnext(NULL, &saved);
    }

    return 0;
}

/**
 * Frees an ENTRY structure if it needs to be feed 
**/

static void
free_entry(ENTRY * en)
{
    if (en->e_gors)
        free(en->e_gors);
    if (en->e_name)
        free(en->e_name);
    if (en->e_addr)
        free(en->e_addr);
}


/**
 * This function checks each address in tmp and formats it
 * properly and copies it over to the new list.
**/

static void
check_and_copy(list_t * to, list_t from)
{
    list_t next = NULL;
    list_t saved = NULL;

    next = list_getnext(from, &saved);
    while (next) {
        insert_entry(to, next->a_data, next->b_data);
        next = list_getnext(NULL, &saved);
    }
}
