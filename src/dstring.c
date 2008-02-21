/**

    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2006 email by Dean Jones
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
#include "dstring.h"
#include "utils.h"

struct _dstring {
    char *str;
    long length;
    long size;
};

const int GROW_SIZE = 100; 

static void
dstring_grow(dstring str, long len)
{
    str->str = xrealloc(str->str, len + GROW_SIZE);
    str->size = len + (GROW_SIZE - 1);
}

dstring
dstring_getNew(long size)
{
    dstring ret = xmalloc(sizeof(struct _dstring));
    ret->str = xmalloc(size+1);
    ret->size = size;
    ret->length = 0;
    return ret;
}

void
dstring_delete(dstring str)
{
    if (str) {
        if (str->str) {
            free(str->str);
        }
        free(str);
    }
}

void
dstring_copyString(dstring str, const char *s, long len)
{
    long new_len = str->length + len;
    if (new_len > str->size) {
        dstring_grow(str, new_len);
    }
    memcpy(str->str + str->length, s, len);
    str->length = new_len;
}

void
dstring_copyChar(dstring str, const char c)
{
    dstring_copyString(str, &c, 1);
}

int
dstring_copyUpTo(dstring str, char stop_char, FILE *in)
{
    int ch = 0;

    while ((ch = fgetc(in)) != EOF) {
        if (ch == stop_char || ch == '\n') {
            break;
        }
        if (ch == '\\') {
            ch = fgetc(in);
            if (ch == '\r') {
                ch = fgetc(in);
            }
            if (ch == '\n') {
                continue;
            }
        }
        dstring_copyChar(str, (char)ch);
    }
    return ch;
}

char *
dstring_getString(dstring str)
{
    return str->str;
}

long 
dstring_getSize(dstring str)
{
    return str->size;
}

long
dstring_getLength(dstring str)
{
    return str->length;
}

