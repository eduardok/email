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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "email.h"
#include "utils.h"
#include "hash.h"

static int nextprime(int);
static unsigned int hash(const char *, unsigned int);
static void empty_table(table_t);
static char *getvaluefromlist(hashval_t, const char *);
static void hash_list_insert(hashval_t *, const char *, const char *);
static void hash_list_free(hashval_t);

/* The actual structure of the hash table */
struct hash_val {
    void *key;
    void *val;
    struct hash_val *next;
};

/**
 * A wrapper for the hash table so that 
 * we can store the size of the table in
 * this structure.
 *
 * typedef as table_t;
**/

struct hash_table {
    unsigned int table_size;
    struct hash_val *hashed;
};


/**
 * return the next prime number out of the number passwd >= n 
**/

static int
nextprime(int n)
{
    int i, div, ceilsqrt = 0;

    for (;; n++) {
        ceilsqrt = ceil(sqrt(n));
        for (i = 2; i <= ceilsqrt; i++) {
            div = n / i;
            if (div * i == n)
                return (n);
        }
    }

    /* Issues ? */
    return (-1);
}

/**
 * take a string and the size of the table and
 * create a value for where to store the key.
**/

static unsigned int
hash(const char *key, unsigned int table_size)
{
    register const char *ptr = key;
    register unsigned int value = 0;

    while (*ptr != '\0')
        value = (value * 37) + (int) *ptr++;

    return (value % table_size);
}

/**
 * clear every area in each part of the hashed array
 * that is inside of the table struct.
**/

static void
empty_table(table_t table)
{
    unsigned int i;

    for (i = 0; i < table->table_size; i++) {
        table->hashed[i].key = NULL;
        table->hashed[i].val = NULL;
        table->hashed[i].next = NULL;
    }
}

/**
 * Transverse the list provided in search of the key
 * provided.  Return the value if they key is found
 * otherwise, return NULL for a failure to find the
 * key in the linked list provided.
**/

static char *
getvaluefromlist(hashval_t top, const char *key)
{
    char *retval = NULL;
    register hashval_t current = top;

    for (; current && !retval; current = current->next) {
        if (strcasecmp(current->key, key) == 0)
            retval = current->val;
    }

    /* 
     * If something was found in the loop
     * it will return here... Otherwise, 
     * retval will be NULL.
     */

    return (retval);
}

/**
 * loop through the lists finding the next free spot to
 * insert an element.  Once it is found, insert the 
 * element properly.
**/

static void
hash_list_insert(hashval_t * top, const char *key, const char *val)
{
    hashval_t new, curr;

    /* If we haven't started a linked list */
    if (*top == NULL) {
        *top = xcalloc(1, sizeof(struct hash_val));
        (*top)->key = xstrdup(key);
        (*top)->val = xstrdup(val);
        (*top)->next = NULL;

        return;
    }

    /* Otherwise, append to the end */
    for (curr = *top; curr->next; curr = curr->next) {
        if (strcasecmp(curr->key, key) == 0)
            return;             /* No need to double hash */
    }

    new = xcalloc(1, sizeof(struct hash_val));
    new->key = xstrdup(key);
    new->val = xstrdup(val);

    new->next = NULL;
    (*top)->next = new;
}

/**
 * free's a hash tables linked lists
**/

static void
hash_list_free(hashval_t top)
{
    hashval_t bak;

    for (; top; top = bak) {
        bak = top->next;
        free(top);
    }
}

/**
 * Initialize the hash table by dynamically allocating
 * an array of size 'size' in the table structure and
 * setting the table_size header.
**/

int
hash_init(table_t * table, int size)
{
    *table = xmalloc(sizeof(struct hash_table));
    (*table)->table_size = nextprime(size);
    (*table)->hashed = xcalloc((*table)->table_size, sizeof(struct hash_val));

    empty_table(*table);
    return (0);
}

/**
 * Lookup a key in the hash table and return it's value 
 * Start by hashing the key.  If we don't find anything
 * in the hashed area, return NULL.  If there is something
 * in the hashed area and it compares to the key passed,
 * set the return value to it's value in the hash table.
 * If there is something in the hashed area and it doesn't
 * compare, walk down the hash table to find it.
 * if the next area is NULL, return NULL because that value
 * does not exist.
**/

char *
hash_lookup_key(table_t table, const char *key)
{
    unsigned int index = 0;
    char *value = NULL;


    index = hash(key, table->table_size);
    if (!table->hashed[index].key)      /* if key doesn't exist */
        return (NULL);

    if (strcasecmp(table->hashed[index].key, key) == 0)
        value = table->hashed[index].val;
    else                        /* Search the linked lists */
        value = getvaluefromlist(table->hashed[index].next, key);


    /* If no value was found in the linked lists */
    if (!value)
        return (NULL);

    return (value);
}

/**
 * Insert a key and value into the hashed array at 
 * a the hash() value area in that array.  If we
 * already have that spot filled, use the linked
 * lists available in each position of the hashed
 * array.  If the linked lists are used, always
 * push the element to the top of the list.
**/

int
hash_insert(table_t * top, const char *key, const char *value)
{
    unsigned int index = 0;

    index = hash(key, (*top)->table_size);

    /* If we have a collision, Insert into the linked lists */
    if ((*top)->hashed[index].val)
        hash_list_insert(&(*top)->hashed[index].next, key, value);
    else {
        (*top)->hashed[index].key = xstrdup(key);
        (*top)->hashed[index].val = xstrdup(value);
    }

    return (0);
}

/**
 * Free all allocated memory in the hashed array
 * and the linked lists.
**/

void
hash_destroy(table_t table)
{
    unsigned int i;

    for (i = 0; i < table->table_size; i++) {
        if (table->hashed[i].key)
            free(table->hashed[i].key);
        if (table->hashed[i].val)
            free(table->hashed[i].val);
        if (table->hashed[i].next)
            hash_list_free(table->hashed[i].next);
    }

    free(table->hashed);
    free(table);
}

/**
 * Print the hash table 
**/

void
hash_print(table_t table)
{
    u_int i;
    hashval_t top;

    for (i = 0; i < table->table_size; i++) {
        printf("Key %s has Value %s\n",
               (char *) table->hashed[i].key, (char *) table->hashed[i].val);
        for (top = table->hashed[i].next; top; top = top->next)
            printf("\tKey %s has Value %s\n", (char *) top->key, (char *) top->val);
    }
}
