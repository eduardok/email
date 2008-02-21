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

#include <sys/types.h>

#include "email.h"
#include "utils.h"
#include "mimeutils.h"

/**
 * executes the 'file' command with the -bi options.
 * it redirects stderr to /dev/null.  If the command
 * didn't execute properly, or the type returned is
 * something that does not look like a mime type, then
 * application/unknown is returned.
**/

#define MAGIC_FILE EMAIL_DIR "/mime-magic.mime"
#define COMMAND "file -m " MAGIC_FILE

char *
mime_filetype(const char *filename)
{
    FILE *bin;
    char *exec;
    char *type;
    char buf[100] = { 0 };

    assert(filename != NULL);

    /* Plus 15 for spaces and ' 2> /dev/null' and also a space for NUL */
    exec = xmalloc(strlen(COMMAND) + strlen(filename) + 15);
    sprintf(exec, "%s '%s' 2> /dev/null", COMMAND, filename);

    bin = popen(exec, "r");
    if (!bin)
        goto DEFAULT_EXIT;

    while (fgets(buf, sizeof(buf), bin)) {
        /**
         * Make sure we don't loop again if we
         * are already at EOF.
        **/

        if (feof(bin))
            break;
    }

    pclose(bin);
    free(exec);
    chomp(buf);

    /* Get past the name specified */
    type = strchr(buf, ':');
    if (!type)
        goto DEFAULT_EXIT;

    /* Get past ':' and any spaces */
    type++;
    while (is_blank(*type))
        type++;

    /* Nothing found  OR not in MIME format of type/subtype */
    if (!strchr(type, '/'))
        goto DEFAULT_EXIT;

    /* If a comma is found, only give portion before comma */
    if (strchr(type, ','))
        return (xstrdup(strtok(type, ",")));

    /* if we are here, just return the string as is */
    return (xstrdup(type));

  DEFAULT_EXIT:
    return (xstrdup("application/unknown"));
}


/**
 * get the name of the file going to be attached from an absolute path 
**/

char *
mime_filename(char *in_name)
{
    char *nameptr;

    nameptr = strrchr(in_name, '/');
    if (nameptr)
        return (++nameptr);

    return (in_name);
}

/**
 * Makes a boundary for Mime emails 
**/

char *
mime_make_boundary(char *buf, size_t size)
{
    char *tmp = buf;

    strncpy(buf, "=-", size);

    tmp++;
    tmp++;                      /* get past the =- we just added */
    random_string(tmp, size - 2);

    return (buf);
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
static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz" "0123456789+/";

/**
 * encode 3 8-bit binary bytes as 4 '6-bit' characters
**/

static void
b64_encodeblock(const u_char in[3], u_char out[4], int len)
{
    out[0] = cb64[in[0] >> 2];
    out[1] = cb64[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
    out[2] = (u_char) (len > 1 ? cb64[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)] : '=');
    out[3] = (u_char) (len > 2 ? cb64[in[2] & 0x3f] : '=');
}

/**
 * Encode_file will encode file infile placing it 
 * in file outfile including padding and EOL of \r\n properly
**/

int
mime_b64_encode_file(FILE *infile, FILE *outfile)
{
    u_char in[3], out[4];
    int i, len, blocksout = 0;

    while (!feof(infile)) {
        len = 0;
        for (i = 0; i < 3; i++) {
            in[i] = (u_char) getc(infile);
            if (!feof(infile))
                len++;
            else
                in[i] = 0;
        }

        if (len) {
            b64_encodeblock(in, out, len);
            for (i = 0; i < 4; i++)
                putc(out[i], outfile);

            blocksout++;
        }

        if (blocksout >= (MAX_B64_LINE / 4) || feof(infile)) {
            if (blocksout)
                fprintf(outfile, "\r\n");

            blocksout = 0;
        }

        if (ferror(infile) || ferror(outfile))
            return (-1);
    }

    return (0);
}

char *
mime_b64_encode_string(const u_char *inbuf, size_t len, u_char *outbuf, size_t size)
{
    u_char encblock[5] = { 0 };

    /* Output buffer is not large enough */
    if (size < (len * (len / 3)) + 2)
        return (NULL);

    while (len) {
        if (len > 3) {
            b64_encodeblock(inbuf, encblock, 3);
            inbuf += 3;
            len -= 3;
        }
        else {
            b64_encodeblock(inbuf, encblock, len);
            len -= len;
        }

        safeconcat(outbuf, encblock, size);
    }

    return (outbuf);
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
qp_is_encodable(int c)
{
    if (((c >= 9) && (c <= 60)) || ((c >= 62) && (c <= 126)))
        return (0);
    return (1);
}

static void
qp_stdout(int ch, int *curr_len, FILE *out)
{
    if (*curr_len == (QP_MAX_LINE_LEN - 1)) {
        fprintf(out, "=\r\n");
        *curr_len = 0;
    }

    fprintf(out, "%c", ch);
    (*curr_len)++;
}

static void
qp_encout(int ch, int *curr_len, FILE *out)
{
    if ((*curr_len + 3) >= QP_MAX_LINE_LEN) {
        fprintf(out, "=\r\n");
        *curr_len = 0;
    }

    fprintf(out, "=%02X", ch);
    *curr_len += 3;
}

/**
 * Encode a quoted printable string.
**/

static void
qp_encode_str(char *str, int *len, FILE *out)
{
    int line_len = *len;

    for (; *str != '\0'; str++) {
        if (line_len == (QP_MAX_LINE_LEN - 1)) {
            fprintf(out, "=\r\n");
            line_len = 0;
        }

        switch (*str) {
            case ' ':
            case '\t':
                if ((str[1] == '\r') || (str[1] == '\n'))
                    qp_encout(*str, &line_len, out);
                else
                    qp_stdout(*str, &line_len, out);
                break;

            case '\r':
                str++;          /* Get to newline */
            case '\n':
                fprintf(out, "\r\n");
                line_len = 0;
                break;

            default:
                if (qp_is_encodable(*str))
                    qp_encout(*str, &line_len, out);
                else
                    qp_stdout(*str, &line_len, out);
                break;
        }
    }

    /* Reset length */
    *len = line_len;
}

int
mime_qp_encode_file(FILE *in, FILE *out)
{
    int line_len = 0;
    char buf[MAXBUF] = { 0 };

    while (fgets(buf, sizeof(buf), in))
        qp_encode_str(buf, &line_len, out);

    if (ferror(in) || ferror(out))
        return (-1);

    return (0);
}
