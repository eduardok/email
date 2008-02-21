#ifndef DSTRING_H_
#define DSTRING_H_

#include <stdio.h>

typedef struct _dstring *dstring;

dstring dstring_getNew(long size);
void dstring_delete(dstring str);

void dstring_copyString(dstring str, const char *s, long len);
void dstring_copyChar(dstring str, const char c);
int dstring_copyUpTo(dstring str, char stop_char, FILE *in);
char *dstring_getString(dstring str);
long dstring_getSize(dstring str);
long dstring_getLength(dstring str);

#endif
