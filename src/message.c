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
#include <unistd.h>
#include <fcntl.h>

#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Autoconf manual suggests this. */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "email.h"
#include "execgpg.h"
#include "utils.h"
#include "file_io.h"
#include "addy_book.h"
#include "remotesmtp.h"
#include "addr_parse.h"
#include "message.h"
#include "mimeutils.h"
#include "error.h"

static int make_gpg_message(const char *, const char *, const char *);
static int make_message(FILE *, FILE *, const char *);
static FILE *create_encrypted_email(const char *);
static FILE *create_signed_email(const char *);
static FILE *create_plain_email(const char *);

static void print_headers(const char *, FILE *);
static void print_mime_headers(const char *, FILE *);
static void print_to_headers(list_t, list_t, FILE *);
static void print_bcc_headers(list_t, FILE *);
static void print_from_headers(char *, char *, FILE *);
static void print_reply_headers(const char *, FILE *);
static void print_subject_headers(const char *, FILE *);
static void print_date_headers(FILE *);
static void print_mailer_headers(FILE *);
static void print_priority_headers(int, FILE *);
static void print_extra_headers(list_t headers, FILE *);

static int attach_files(const char *, FILE *);

/**
 * This function takes the current content that was copied
 * in to us and creates a final message with the email header
 * and the appended content.  It will also attach any files
 * that were specified at the command line.
**/

static void
print_headers(const char *border, FILE * message)
{
    char *sm_bin = NULL;
    char *smtp_serv = NULL;
    char *reply_to = NULL;
    char *user_name = NULL;
    char *email_addr = NULL;

    user_name = get_conf_value("MY_NAME");
    email_addr = get_conf_value("MY_EMAIL");
    sm_bin = get_conf_value("SENDMAIL_BIN");
    smtp_serv = get_conf_value("SMTP_SERVER");
    reply_to = get_conf_value("REPLY_TO");

    print_subject_headers(Mopts.subject, message);
    print_from_headers(user_name, email_addr, message);
    print_to_headers(Mopts.to, Mopts.cc, message);

    /**
     * We want to check here to see if we are sending mail by invoking sendmail
     * If so, We want to add the BCC line to the headers.  Sendmail checks this
     * Line and makes sure it sends the mail to the BCC people, and then remove
     * the BCC addresses...  Keep in mind that sending to an smtp servers takes
     * presidence over sending to sendmail incase both are mentioned.
    **/

    if (sm_bin && !smtp_serv)
        print_bcc_headers(Mopts.bcc, message);

    /* The rest of the standard headers */
    print_date_headers(message);
    print_reply_headers(reply_to, message);
    print_mime_headers(border, message);
    print_mailer_headers(message);
    print_priority_headers(Mopts.priority, message);
    print_extra_headers(Mopts.headers, message);

    fprintf(message, "\r\n");

}

/**
 * All functions below are pretty self explanitory.  
 * They basically just preform some simple header printing 
 * for our email 'message'.  I'm not doing to comment every 
 * function from here because you should be able to read the 
 * damn functions and get a great idea
**/

static void
print_mime_headers(const char *b, FILE * message)
{
    if (Mopts.encrypt) {
        fprintf(message, "Mime-Version: 1.0\r\n");
        fprintf(message, "Content-Type: multipart/encrypted; "
                "protocol=\"application/pgp-encrypted\"; " "boundary=\"%s\"\r\n", b);
    }
    else if (Mopts.sign) {
        fprintf(message, "Mime-Version: 1.0\r\n");
        fprintf(message, "Content-Type: multipart/signed; "
                "micalg=pgp-sha1; protocol=\"application/pgp-signature\"; "
                "boundary=\"%s\"\r\n", b);
    }
    else if (Mopts.attach) {
        fprintf(message, "Mime-Version: 1.0\r\n");
        fprintf(message, "Content-Type: multipart/mixed; boundary=\"%s\"\r\n", b);
    }
    else {
        if (Mopts.html) {
            fprintf(message, "Mime-Version: 1.0\r\n");
            fprintf(message, "Content-Type: text/html\r\n");
        }
        else {
            fprintf(message, "Content-Type: text/plain\r\n");
        }
    }

}

/**
 * just prints the to headers, and cc headers if available
**/

static void
print_to_headers(list_t to, list_t cc, FILE *message)
{
    list_t next = NULL;
    list_t saved = NULL;

    assert(to != NULL);

    /* Print To Headers */
    fprintf(message, "To: ");
    next = list_getnext(to, &saved);
    while (next) {
        char buf[MINBUF] = {0};
        format_email_addr(next->a_data, next->b_data, buf, MINBUF-1);
        fprintf(message, "%s", buf);
        next = list_getnext(NULL, &saved);
        if (next != NULL) {
            fprintf(message, ", ");
        } else {
            fprintf(message, "\r\n");
        }
    }

    /* if Cc is not equal to NULL, print cc headers */
    if (cc != NULL) {
        fprintf(message, "Cc: ");
        next = list_getnext(cc, &saved);
        while (next) {
            char buf[MINBUF] = {0};
            format_email_addr(next->a_data, next->b_data, buf, MINBUF-1);
            fprintf(message, "%s", buf);
            next = list_getnext(NULL, &saved);
            if (next != NULL) {
                fprintf(message, ", ");
            } else {
                fprintf(message, "\r\n");
            }
        }
    }
}

/**
 * Bcc gets a special function because it's not always printed for standard
 * Mail delivery.  Only if sendmail is going to be invoked, shall it be printed
 * reason being is because sendmail needs the Bcc header to know everyone who
 * is going to recieve the message, when it is done with reading the Bcc headers
 * it will remove this from the headers field.
**/

static void
print_bcc_headers(list_t bcc, FILE *message)
{
    list_t next = NULL;
    list_t saved = NULL;

    if (bcc != NULL) {
        fprintf(message, "Bcc: ");
        next = list_getnext(bcc, &saved);
        while (next) {
            char buf[MINBUF] = {0};
            format_email_addr(next->a_data, next->b_data, buf, MINBUF-1);
            fprintf(message, "%s", buf);
            next = list_getnext(NULL, &saved);
            if (next != NULL) {
                fprintf(message, ", ");
            } else {
                fprintf(message, "\r\n");
            }
        }
    }
}

/** Print From Headers **/

static void
print_from_headers(char *name, char *address, FILE *message)
{
    char buf[MINBUF] = { 0 };

    format_email_addr(name, address, buf, MINBUF - 1);
    fprintf(message, "From: %s\r\n", buf);
}

/** Print the Reply To Header **/

static void
print_reply_headers(const char *address, FILE * message)
{
    if (address)
        fprintf(message, "Reply-To: <%s>\r\n", address);
}

/** Subject Headers **/

static void
print_subject_headers(const char *subject, FILE * message)
{
    if (subject)
        fprintf(message, "Subject: %s\r\n", subject);
}

/** Print Date Headers **/

static void
print_date_headers(FILE * message)
{
    time_t set_time;
    struct tm *lt;
    char buf[MINBUF] = { 0 };

    set_time = time(&set_time);

#ifdef USE_GMT
    lt = gmtime(&set_time);
#else
    lt = localtime(&set_time);
#endif

#ifdef USE_GNU_STRFTIME
    strftime(buf, MINBUF, "%a, %d %b %Y %H:%M:%S %z", lt);
#else
    strftime(buf, MINBUF, "%a, %d %b %Y %H:%M:%S %Z", lt);
#endif

    fprintf(message, "Date: %s\r\n", buf);
}

/** Print Our Mailer... ( Exploit the headers and promote our product ) **/

static void
print_mailer_headers(FILE * message)
{
    struct utsname sys;

    uname(&sys);
    fprintf(message, "X-Mailer: email v%s (%s %s %s [http://email.cleancode.org])\r\n",
            EMAIL_VERSION, sys.sysname, sys.release, sys.machine);
}

/** Print Priority Headers **/

static void
print_priority_headers(int priority, FILE * message)
{
    if (priority)
        fprintf(message, "X-Priority: 1\r\n");
}


static void
print_extra_headers(list_t headers, FILE *message)
{
    list_t saved = NULL;
    list_t next = NULL;
    next = list_getnext(headers, &saved);
    while (next) {
        fprintf(message, "%s\r\n", next->a_data);
        next = list_getnext(NULL, &saved);
    }
}

/**
 * set up the appropriate MIME and Base64 headers for 
 * the attachment of file specified in Mopts.attach
**/

static int
attach_files(const char *boundary, FILE *out)
{
    list_t next_file = NULL;
    list_t saved = NULL;
    char *file_name = NULL;
    char *file_type = NULL;

    /*
     * What we will do here is parse Mopts.attach for comma delimited file
     * names.  If there was only one file specified with no comma, then strtok()
     * will just return that file and the next call to strtok() will be NULL
     * which will allow use to break out of our loop of attaching base64 stuff.
     */

    next_file = list_getnext(Mopts.attach, &saved);
    while (next_file) {
        FILE *current;

        if (!(current = fopen(next_file->a_data, "r"))) {
            fatal("Could not open attachment: %s", next_file->a_data);
            return (ERROR);
        }

        /* If the user specified an absolute path, just get the file name */
        file_type = mime_filetype(next_file->a_data);
        file_name = mime_filename(next_file->a_data);

        /* Set our MIME headers */
        fprintf(out, "\r\n--%s\r\n", boundary);
        fprintf(out, "Content-Transfer-Encoding: base64\r\n");
        fprintf(out, "Content-Type: %s; name=\"%s\"\r\n", file_type, file_name);
        fprintf(out, "Content-Disposition: attachment; filename=\"%s\"\r\n", file_name);
        fprintf(out, "\r\n");

        /* Encode to 'out' */
        mime_b64_encode_file(current, out);

        fclose(current);
        free(file_type);
        current = NULL;
        next_file = list_getnext(NULL, &saved);
    }

    return (TRUE);
}

/** 
 * Makes a standard plain text message while taking into
 * account the MIME message types and boundary's needed
 * if and when a file is attached.
**/

static int
make_message(FILE * in, FILE * out, const char *border)
{
    char buf[998] = { 0 };      /* max size of RFC821 line - 2 */

    if (!in || !out)
        return (-1);

    if (Mopts.attach) {
        fprintf(out, "--%s\r\n", border);

        if (Mopts.html)
            fprintf(out, "Content-Type: text/html\r\n\r\n");
        else
            fprintf(out, "Content-Type: text/plain\r\n\r\n");
    }

    while (fgets(buf, sizeof(buf), in)) {
        chomp(buf);
        fprintf(out, "%s\r\n", buf);
    }

    if (ferror(in))
        return (-1);

    if (Mopts.attach) {
        attach_files(border, out);
        fprintf(out, "\r\n\r\n--%s--\r\n", border);
    }

    return (0);
}

/**
 * Makes a message type specifically for gpg encryption and 
 * signing.  Specific MIME message descriptions are needed
 * when signing/encrypting a file before it is actuall signed
 * or encrypted.  This function takes care of that.
**/

static int
make_gpg_message(const char *in, const char *out, const char *border)
{
    FILE *infd, *outfd;

    infd = fopen(in, "r");
    if (!infd)
        return (-1);

    outfd = fopen(out, "w+");
    if (!outfd)
        return (-1);

    if (Mopts.attach) {
        fprintf(outfd, "Content-Type: multipart/mixed; boundary=\"%s\"\r\n\r\n", border);
        fprintf(outfd, "\r\n--%s\r\n", border);
    }

    if (Mopts.html)
        fprintf(outfd, "Content-Type: text/html\r\n");
    else
        fprintf(outfd, "Content-Type: text/plain\r\n");

    fprintf(outfd, "Content-Transfer-Encoding: quoted-printable\r\n\r\n");
    mime_qp_encode_file(infd, outfd);

    if (Mopts.attach) {
        attach_files(border, outfd);
        fprintf(outfd, "\r\n--%s--\r\n", border);
    }

    fclose(infd);
    fclose(outfd);

    return (0);
}

/**
 * Creates an encrypted message with the appropriate
 * MIME message types.  Once it is done with everything
 * it will rewind the file position indicator to the top
 * of the file and returns the open file descriptor
**/

static FILE *
create_encrypted_email(const char *file)
{
    FILE *ret, *tmp;
    char buf[MINBUF] = { 0 };
    char tmpfile[MINBUF] = { 0 };
    char encfile[MINBUF] = { 0 };
    char border1[15] = { 0 };
    char border2[15] = { 0 };

    assert(file != NULL);

    /* Create two borders if we're attaching files */
    mime_make_boundary(border1, sizeof(border1));
    if (Mopts.attach)
        mime_make_boundary(border2, sizeof(border2));

    create_tmpname(tmpfile, sizeof(tmpfile));
    if (make_gpg_message(file, tmpfile, border2) < 0)
        return (NULL);


    /* Create a file name for gpg output */
    create_tmpname(encfile, sizeof(encfile));
    if (Mopts.encrypt && Mopts.sign)
        tmp = call_gpg_encsig(tmpfile, encfile);
    else
        tmp = call_gpg_enc(tmpfile, encfile);
    if (!tmp) {
        fatal("Could not create encrypted email");
        return (NULL);
    }

    /* Reuse tmpfile because we don't need this variable anymore */
    memset(tmpfile, 0, sizeof(tmpfile));
    create_tmpname(tmpfile, sizeof(tmpfile));
    ret = fopen(tmpfile, "w+");
    if (!ret) {
        fclose(tmp);
        return (NULL);
    }

    print_headers(border1, ret);
    fprintf(ret, "--%s\r\n", border1);
    fprintf(ret, "Content-Type: application/pgp-encrypted\r\n\r\n");
    fprintf(ret, "Version: 1\r\n");
    fprintf(ret, "\r\n--%s\r\n", border1);
    fprintf(ret, "Content-Type: application/octet-stream; name=encrypted.asc\r\n\r\n");

    /* Copy in encrypted data */
    while (fgets(buf, sizeof(buf), tmp)) {
        chomp(buf);
        fprintf(ret, "%s\r\n", buf);
    }

    if (ferror(tmp)) {
        fclose(tmp);
        fclose(ret);
        return (NULL);
    }

    fprintf(ret, "\r\n--%s--\r\n", border1);

    fclose(tmp);
    rewind(ret);

    return (ret);
}

/**
 * Creates a signed message with gpg and takes into 
 * account the correct MIME message types to add to 
 * the message.  When this function is done, it will
 * rewind the file position to the top of the file and
 * return the open file
**/

static FILE *
create_signed_email(const char *file)
{
    FILE *ret = NULL;
    FILE *tmp = NULL;
    FILE *data = NULL;
    char buf[MINBUF] = { 0 };
    char tmpfile[MINBUF] = { 0 };
    char encfile[MINBUF] = { 0 };
    char border1[15] = { 0 };
    char border2[15] = { 0 };

    assert(file != NULL);

    /* Create two borders if we're attaching files */
    mime_make_boundary(border1, sizeof(border1));
    if (Mopts.attach)
        mime_make_boundary(border2, sizeof(border2));

    create_tmpname(tmpfile, sizeof(tmpfile));
    if (make_gpg_message(file, tmpfile, border2) < 0)
        return (NULL);

    /* Open the formatted message */
    data = fopen(tmpfile, "r");
    if (!data)
        goto BADEXIT;

    /* Create a file name for gpg output */
    create_tmpname(encfile, sizeof(encfile));
    tmp = call_gpg_sig(tmpfile, encfile);
    if (!tmp)
        goto BADEXIT;

    /* Reuse encfile because we don't need this variable anymore */
    memset(encfile, 0, sizeof(encfile));
    create_tmpname(encfile, sizeof(encfile));
    ret = fopen(encfile, "w+");
    if (!ret)
        goto BADEXIT;

    print_headers(border1, ret);

    fprintf(ret, "\r\n--%s\r\n", border1);
    while (fgets(buf, sizeof(buf), data)) {
        chomp(buf);
        fprintf(ret, "%s\r\n", buf);
    }

    if (ferror(data))
        goto BADEXIT;

    fprintf(ret, "\r\n--%s\r\n", border1);
    fprintf(ret, "Content-Type: application/pgp-signature\r\n");
    fprintf(ret, "Content-Description: This is a digitally signed message\r\n\r\n");

    /* Copy in encrypted data */
    while (fgets(buf, sizeof(buf), tmp)) {
        chomp(buf);
        fprintf(ret, "%s\r\n", buf);
    }

    if (ferror(tmp))
        goto BADEXIT;

    fprintf(ret, "\r\n--%s--\r\n", border1);

    fclose(tmp);
    fclose(data);
    rewind(ret);

    return (ret);

    /* This is jumped to incase of errors */
  BADEXIT:
    if (data)
        fclose(data);
    if (tmp)
        fclose(tmp);
    if (ret)
        fclose(ret);
    return (NULL);
}

/**
 * Creates a plain text (or html) email and 
 * specifies the necessary MIME types if needed
 * due to attaching base64 files.
 * when this function is done, it will rewind
 * the file position and return an open file
**/

static FILE *
create_plain_email(const char *file)
{
    FILE *ret, *tmp;
    char ffile[MINBUF] = { 0 };
    char border[10] = { 0 };

    assert(file != NULL);

    if (Mopts.attach)
        mime_make_boundary(border, sizeof(border));

    create_tmpname(ffile, sizeof(ffile));

    tmp = fopen(file, "r");
    if (!tmp)
        return (NULL);

    ret = fopen(ffile, "w+");
    if (!ret)
        return (NULL);

    print_headers(border, ret);

    if (make_message(tmp, ret, border) < 0 || ferror(tmp)) {
        fclose(tmp);
        fclose(ret);
        return (NULL);
    }

    fclose(tmp);
    rewind(ret);

    return (ret);
}

/**
 * this is the function that takes over from main().  
 * It will call all functions nessicary to finish off the 
 * rest of the program and then return properly. 
**/

void
create_mail(void)
{
    FILE *mailer;
    char tempfile[MINBUF] = { 0 };
    char add_subject[MINBUF] = { 0 };


    /* Get our temp file for input purposes only */
    create_tmpname(tempfile, sizeof(tempfile));

    /**
     * first let's check if someone has tried to send stuff in from STDIN 
     * if they have, let's call a read to stdin
    **/
    if (isatty(STDIN_FILENO) == 0) {
        if (read_input(tempfile) == ERROR) {
            fatal("Problem reading from STDIN redirect\n");
            proper_exit(ERROR);
        }
    }
    else {
        /* If they aren't sending a blank email */
        if (!Mopts.blank) {
            /* let's check if they want to add a subject or not */
            if (Mopts.subject == NULL) {
                fprintf(stderr, "Subject: ");
                fgets(add_subject, sizeof(add_subject), stdin);
                chomp(add_subject);
                Mopts.subject = add_subject;
            }

            /* Now we need to let them create a file */
            if (edit_file(tempfile) == ERROR)
                proper_exit(ERROR);
        }
        else {
            int fd;

            /* Just create a blank file */
            fd = creat(tempfile, S_IRUSR | S_IWUSR);
            if (fd < 0) {
                fatal("Could not create tempfile");
                proper_exit(ERROR);
            }

            close(fd);
        }

    }

    /* Create a message according to the type */
    if (Mopts.encrypt)
        mailer = create_encrypted_email(tempfile);
    else if (Mopts.sign)
        mailer = create_signed_email(tempfile);
    else
        mailer = create_plain_email(tempfile);

    if (!mailer) {
        fatal("Could not create email properly");
        proper_exit(ERROR);
    }

    /* Now let's send the message */
    if (sendmail(mailer) == ERROR)
        proper_exit(ERROR);
}

