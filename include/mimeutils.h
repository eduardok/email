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
#ifndef _MIMEUTILS_H
#define _MIMEUTILS_H  1


char *mime_make_boundary(char *, size_t);
char *mime_filetype(const char *);
char *mime_filename(char *);
int mime_qp_encode_file(FILE *, FILE *);
int mime_b64_encode_file(FILE *, FILE *);
char *mime_b64_encode_string(const u_char *, size_t, u_char *, size_t);

#endif /* _MIMEUTILS_H */
