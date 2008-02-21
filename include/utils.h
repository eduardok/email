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
#ifndef __UTILS_H
#define __UTILS_H   1

#include <sys/types.h>
#include "linked_lists.h"

int is_blank(int);
int safeconcat(char *, const char *, size_t);
void expand_path(char *, char *, size_t);
void chomp(char *);
void *xrealloc(void *, size_t);
void *xmalloc(size_t);
void *xcalloc(size_t, size_t);
char *xstrdup(const char *);
int copyfile(const char *, const char *);
void dead_letter(void);
void random_string(char *, size_t);
char *getfirstemail(void);
char *create_tmpname(char *, size_t);
void destroy_tmpfiles(list_t);
void set_conf_value(const char *, const char *);
char *get_conf_value(const char *);
void proper_exit(int);
int explode_to_list(char *str, const char *del, list_t *ret);

#endif /* __UTILS_H */
