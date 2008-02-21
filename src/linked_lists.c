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
#include "utils.h"
#include "addr_parse.h"
#include "linked_lists.h"
#include "error.h"



/**
 * Copy 'from' list to 'to' list
**/
void
list_copy(list_t *to, list_t from)
{
    list_t current = NULL;

    for (current = from; current; current = current->next) {
        if (validate_email(current->b_data) == ERROR) {
            warning("Email address: %s is not valid: Skipping\n", current->b_data);
            continue;
        }

        list_insert(to, current->a_data, current->b_data);
    }
}

/**
 * Create a new node and and link it up with the rest
**/

void
list_insert(list_t *ref, char *a_data, char *b_data)
{
    list_t current;
    list_t ret;

    ret = xmalloc(sizeof(struct node));
    ret->next = NULL;
    if (a_data) {
        ret->a_data = xstrdup(a_data);
    } else {
        ret->a_data = NULL;
    }

    if (b_data) {
        ret->b_data = xstrdup(b_data);
    } else {
        ret->b_data = NULL;
    }

    if (*ref == NULL) {
        *ref = ret;
        return;
    }

    for (current = *ref; current->next; current = current->next) {
        ; /* Just get to the end of the list. */
    }
    current->next = ret;
}

/**
 * Get next pointer from the list.
 * First call to this function should contain a valid list_t type
 * and a list_t pointer to pointer to hold the current position.
 * Each subsequent call to this function should set the first
 * argument to NULL, yet set the second argument to the past 
 * pointer used in the last call.
 * Will return NULL when at end of list.
**/

list_t 
list_getnext(list_t ref, list_t *curr)
{
    if (!ref) {
        if (*curr == NULL)
            return (NULL);

        /* If the next pointer is not NULL */
        if ((*curr = (*curr)->next)) {
            return *curr;
        } else {
            return NULL;
        }
    }

    *curr = ref;
    return *curr;
}

/**
 * Free the entire list 
**/

void
list_free(list_t first)
{
    list_t tmp;

    for (; first; first = tmp) {
        tmp = first->next;
        if (first->a_data) {
            free(first->a_data);
        }
        if (first->b_data) {
            free(first->b_data);
        }
        free(first);
    }
}


/**
 * Return the first element in the list
**/

list_t 
list_gettop(list_t top)
{
    list_t retval = NULL;

    if (top) {
        retval = top;
    }
    return retval;
}

/**
 * Removes the top element from the list
**/

void
list_pop(list_t *list)
{
    list_t tmp = NULL;

    tmp = (*list)->next;
    free(*list);
    *list = tmp;
}

