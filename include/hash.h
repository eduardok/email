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
#ifndef __HASH_H
#define __HASH_H  1

typedef struct hash_val *hashval_t;
typedef struct hash_table *table_t;


int hash_init(table_t *, int);
char *hash_lookup_key(table_t, const char *);
int hash_insert(table_t *, const char *, const char *);
void hash_destroy(table_t);
void hash_print(table_t);

#endif /* __HASH_H */
