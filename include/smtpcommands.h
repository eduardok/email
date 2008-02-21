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
#ifndef __SMTPCOMMANDS_H
#define __SMTPCOMMANDS_H  1

#include "ipfunc.h"
#include "linked_lists.h"       /* for list_t type */

char *smtp_errstring(void);
int init_smtp_auth(SOCKET *, const char *, const char *, const char *);
int init_smtp(SOCKET *, const char *);
int set_smtp_mail_from(SOCKET *, const char *);
int set_smtp_recipients(SOCKET *, list_t, list_t, list_t);
int send_smtp_data(SOCKET *, FILE *);
int close_smtp_connection(SOCKET *);

#endif /* __SMTPCOMMANDS_H */
