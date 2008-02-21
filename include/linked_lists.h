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
#ifndef LINKED_LISTS
#define LINKED_LISTS  1

/* Definition of our linked lists */
struct node {
    char *a_data;
    char *b_data;
    struct node *next;
};
typedef struct node *list_t;

void list_copy(list_t *, list_t);
void list_insert(list_t *, char *a_data, char *b_data);
list_t list_getnext(list_t, list_t *);
void list_free(list_t);
list_t list_gettop(list_t);
void list_pop(list_t *);


#endif /* LINKED_LISTS */
