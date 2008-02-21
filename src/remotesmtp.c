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
#include <signal.h>

#include "email.h"
#include "utils.h"
#include "message.h"
#include "file_io.h"
#include "remotesmtp.h"
#include "processmail.h"
#include "error.h"

/* Local Function Declarations */
static int save_sent_email(FILE *);

/**
 * This function does all the required SMTP connection 
 * and commands. It will send the e-mail we specified 
 * and use the remote smtp server if there is one, otherwise 
 * it will get it out of the config variable 
**/

int
sendmail(FILE * mail)
{
    int smtp_port;
    char *smtp_serv, *sm_bin;

    smtp_serv = get_conf_value("SMTP_SERVER");
    sm_bin = get_conf_value("SENDMAIL_BIN");

    if (smtp_serv) {
        smtp_port = atoi(get_conf_value("SMTP_PORT"));
        if (process_remote(smtp_serv, smtp_port, mail) == ERROR)
            return (ERROR);
    }
    else if (sm_bin) {
        if (process_internal(sm_bin, mail) == ERROR)
            return (ERROR);
    }
    else {
        fprintf(stderr, "No SMTP server specified!\n");
        return (ERROR);
    }

    if (save_sent_email(mail) == ERROR)
        return (ERROR);

    fclose(mail);
    return (TRUE);
}

/**
 * will save the sent email if a place is specified in the 
 * configuration file It will append each email to one particular 
 * file called 'email.sent'
**/

static int
save_sent_email(FILE * final_email)
{
    FILE *save;
    char buf[MINBUF] = { 0 };
    char *save_file = NULL;
    char resolved_path[MINBUF] = { 0 };

    save_file = get_conf_value("SAVE_SENT_MAIL");
    if (!save_file)
        return (FALSE);

    expand_path(resolved_path, save_file, MINBUF);
    safeconcat(resolved_path, "/email.sent", MINBUF);

    if (!(save = fopen(resolved_path, "a"))) {
        warning("Could not open file: %s", resolved_path);
        return (ERROR);
    }

    fseek(final_email, SEEK_SET, 0);
    fputs("\n", save);

    while (fgets(buf, sizeof(buf), final_email) != NULL)
        fputs(buf, save);

    /* In case fgets returned NULL for errror */
    if (ferror(final_email)) {
        fclose(save);
        return (ERROR);
    }

    fflush(save);
    fclose(save);

    return (TRUE);
}
