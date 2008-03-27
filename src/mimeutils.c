/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
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

#include <sys/types.h>

#include "email.h"
#include "utils.h"
#include "dstrbuf.h"
#include "dutil.h"
#include "mimeutils.h"

static dstrbuf *
getMimeType(const char *str)
{
	dstrbuf *ret = DSB_NEW;
	while (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\0') {
		dsbnCat(ret, str, 1);
		str++;
	}
	return ret;
}

/**
 * get the name of the file going to be attached from an absolute path 
**/
dstrbuf *
mimeFilename(const char *in_name)
{
	char *nameptr=NULL;
	dstrbuf *ret = DSB_NEW;

	nameptr = strrchr(in_name, '/');
	if (nameptr) {
		dsbCopy(ret, ++nameptr);
	} else {
		dsbCopy(ret, in_name);
	}
	return ret;
}

/**
 * executes the 'file' command with the -bi options.
 * it redirects stderr to /dev/null.  If the command
 * didn't execute properly, or the type returned is
 * something that does not look like a mime type, then
 * application/unknown is returned.
**/
#define MAGIC_FILE EMAIL_DIR "/mime-magic.mime"

dstrbuf *
mimeFiletype(const char *filename)
{
	bool found=false;
	int i=0, veclen=0;
	dstrbuf *type=NULL;
	dstrbuf *buf=DSB_NEW;
	dvector tmpvec=NULL, vec=NULL;
	const char *ext=NULL;
	dstrbuf *filen=NULL;
	FILE *file = fopen(MAGIC_FILE, "r");

	if (!file) {
		goto exit;
	}
	filen = mimeFilename(filename);
	tmpvec = explode(filen->str, ".");
	dsbDestroy(filen);
	if (!tmpvec) {
		goto exit;
	}
	ext = tmpvec[1];
	while (!feof(file)) {
		dsbReadline(buf, file);
		if (buf->str[0] == '#' || buf->str[0] == '\n') {
			continue;
		}
		type = getMimeType(buf->str);
		if (!type) {
			continue;
		}
		vec = explode(buf->str, " \t");
		veclen = dvLength(vec);
		/* Start i at 1 since the first element in the
		 * vector is the mime type. The exts are after that. */
		for (i=1; i < veclen; i++) {
			if (strcmp((char *)vec[i], ext) == 0) {
				found = true;
				break;
			}
		}
		dvDestroy(vec);
		if (found) {
			/* Found it! */
			break;
		} else {
			dsbDestroy(type);
		}
	}

exit:
	dvDestroy(tmpvec);
	dsbDestroy(buf);
	fclose(file);
	if (!type) {
		type = DSB_NEW;
		dsbCopy(type, "application/unknown");
	}
	return type;
}

/**
 * Makes a boundary for Mime emails 
**/
dstrbuf *
mimeMakeBoundary(void)
{
	dstrbuf *buf=DSB_NEW;
	dstrbuf *rstr=randomString(15);
	dsbPrintf(buf, "=-%s", rstr->str);
	return buf;
}

/**
 * This base64 bit of code was taken from a program written by: Bob Trower
 * I didn't see a point in reinvinting the wheel here, so I found the best code for
 * this portion and implimented it to suit the best needs of our program.
 *
 * To find out more about Bob Trower and his b64.c project, go to
 * http://base64.sourceforge.net
**/

#define MAX_B64_LINE 72

/* Our base64 table of chars */
static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
			   "abcdefghijklmnopqrstuvwxyz" 
			   "0123456789+/";

/**
 * encode 3 8-bit binary bytes as 4 '6-bit' characters
**/

static void
mimeB64EncodeBlock(const u_char in[3], u_char out[4], int len)
{
	out[0] = cb64[in[0] >> 2];
	out[1] = cb64[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
	out[2] = (u_char) (len > 1 ? cb64[((in[1] & 0x0f) << 2) | 
		 ((in[2] & 0xc0) >> 6)] : '=');
	out[3] = (u_char) (len > 2 ? cb64[in[2] & 0x3f] : '=');
}


/**
 * Encode_file will encode file infile placing it 
 * in file outfile including padding and EOL of \r\n properly
**/
int
mimeB64EncodeFile(FILE *infile, dstrbuf *outbuf)
{
	u_char in[3], out[4];
	int i, len, blocksout = 0;

	while (!feof(infile)) {
		len = 0;
		for (i = 0; i < 3; i++) {
			in[i] = (u_char) getc(infile);
			if (!feof(infile)) {
				len++;
			} else {
				in[i] = 0;
			}
		}
		if (len) {
			mimeB64EncodeBlock(in, out, len);
			dsbnCat(outbuf, (char *)out, 4);
			blocksout++;
		}
		if (blocksout >= (MAX_B64_LINE / 4) || feof(infile)) {
			if (blocksout) {
				dsbCat(outbuf, "\r\n");
			}
			blocksout = 0;
		}
		if (ferror(infile)) {
			return -1;
		}
	}
	return 0;
}

/**
 * Encode a string into base64.
 */
dstrbuf *
mimeB64EncodeString(const u_char *inbuf, size_t len)
{
	dstrbuf *retbuf = dsbNew(100);
	u_char encblock[5] = {0};

	while (len) {
		if (len > 3) {
			mimeB64EncodeBlock(inbuf, encblock, 3);
			inbuf += 3;
			len -= 3;
		}
		else {
			mimeB64EncodeBlock(inbuf, encblock, len);
			len -= len;
		}
		dsbCat(retbuf, (char *)encblock);
	}
	return retbuf;
}

/* RFC 2045 standard line length not counting CRLF */
#define QP_MAX_LINE_LEN 76

/* Max size of a buffer including CRLF and NUL */
#define MAX_QP_BUF 79

/**
 * RFC 2045 says:
 * chars 9 - 32 (if not trailing end-of-line)
 * chars 33 - 60
 * chars 62 - 126
 * can be represented as-is.  All others 
 * should be encoded.
**/

static int
qpIsEncodable(int c)
{
	if (((c >= 9) && (c <= 60)) || ((c >= 62) && (c <= 126))) {
		return 0;
	}
	return 1;
}

static void
qpStdout(int ch, int *curr_len, dstrbuf *out)
{
	if (*curr_len == (QP_MAX_LINE_LEN - 1)) {
		dsbPrintf(out, "=\r\n");
		*curr_len = 0;
	}

	dsbPrintf(out, "%c", ch);
	(*curr_len)++;
}

static void
qpEncout(int ch, int *curr_len, dstrbuf *out)
{
	if ((*curr_len + 3) >= QP_MAX_LINE_LEN) {
		dsbPrintf(out, "=\r\n");
		*curr_len = 0;
	}

	dsbPrintf(out, "=%02X", ch);
	*curr_len += 3;
}

/**
 * Encode a quoted printable string.
**/
void
mimeQpEncodeStr(dstrbuf *in, dstrbuf *out)
{
	int line_len = in->len;
	char *str = in->str;

	for (; *str != '\0'; str++) {
		if (line_len == (QP_MAX_LINE_LEN - 1)) {
			dsbPrintf(out, "=\r\n");
			line_len = 0;
		}

		switch (*str) {
		case ' ':
		case '\t':
			if ((str[1] == '\r') || (str[1] == '\n')) {
				qpEncout(*str, &line_len, out);
			} else {
				qpStdout(*str, &line_len, out);
			}
			break;
		case '\r':
			str++;          /* Get to newline */
		case '\n':
			dsbPrintf(out, "\r\n");
			line_len = 0;
			break;
		default:
			if (qpIsEncodable(*str)) {
				qpEncout(*str, &line_len, out);
			} else {
				qpStdout(*str, &line_len, out);
			}
			break;
		}
	}
}

