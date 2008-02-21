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
#include <signal.h>

#include "getopt.h"

#include "email.h"
#include "conf.h"
#include "utils.h"
#include "addy_book.h"
#include "file_io.h"
#include "message.h"
#include "error.h"
#include "mimeutils.h"

static void module_usage(const char *);

static struct option Gopts[] = {
    {"attach", 1, 0, 'a'},
    {"attachment", 1, 0, 'a'},
    {"quiet", 0, 0, 'q'},
    {"high-priority", 0, 0, 'o'},
    {"encrypt", 0, 0, 'e'},
    {"html", 0, 0, 1},
    {"sign", 0, 0, 2},
    {"clearsign", 0, 0, 2},
    {"cc", 1, 0, 3},
    {"bcc", 1, 0, 4},
    {"subject", 1, 0, 's'},
    {"sub", 1, 0, 's'},
    {"smtp-server", 1, 0, 'r'},
    {"smtp-port", 1, 0, 'p'},
    {"conf-file", 1, 0, 'c'},
    {"check-config", 0, 0, 't'},
    {"version", 0, 0, 'v'},
    {"blank-mail", 0, 0, 'b'},
    {"smtp-user", 1, 0, 'u'},
    {"smtp-username", 1, 0, 'u'},
    {"smtp-pass", 1, 0, 'i'},
    {"smtp-password", 1, 0, 'i'},
    {"smtp-auth", 1, 0, 'm'},
    {"gpg-pass", 1, 0, 'g'},
    {"gpg-password", 1, 0, 'g'},
    {"from-addr", 1, 0, 'f'},
    {"from-name", 1, 0, 'n'},
    {"header", 1, 0, 'H'},
    {"to-name", 1, 0, 5}
};

int
main(int argc, char **argv)
{
    int get;
    int opt_index = 0;          /* for getopt */
    char *cc_string = NULL;
    char *bcc_string = NULL;
    const char *opts = "f:n:a:p:oqedvtb?c:s:r:u:i:g:m:";

    /* Set certian global options to NULL */
    conf_file = NULL;
    memset(&Mopts, 0, sizeof(struct mailer_options));

    /* Check if they need help */
    if ((argc > 1) && (!strcmp(argv[1], "-h") ||
                       !strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"))) {
        if (argc == 3)
            module_usage(argv[2]);
        else if (argc == 2)
            usage();
        else {
            fprintf(stderr, "Only specify one option with %s: \n", argv[1]);
            usage();
        }
    }

    hash_init(&table, 28);
    if (!table) {
        fprintf(stderr, "ERROR: Could not initialize Hash table.\n");
        exit(0);
    }

    while ((get = getopt_long_only(argc, argv, opts, Gopts, &opt_index)) > EOF) {
        switch (get) {
            case 'n':
                set_conf_value("MY_NAME", optarg);
                break;

            case 'f':
                set_conf_value("MY_EMAIL", optarg);
                break;

            case 'a':
                explode_to_list(optarg, ",", &Mopts.attach);
                break;

            case 'q':
                quiet = 1;
                break;

            case 'p':
                set_conf_value("SMTP_PORT", optarg);
                break;

            case 'o':
                Mopts.priority = 1;
                break;

            case 'e':
                Mopts.encrypt = 1;
                break;

            case 's':
                Mopts.subject = optarg;
                break;

            case 'r':
                set_conf_value("SMTP_SERVER", optarg);
                break;

            case 'c':
                conf_file = optarg;
                break;

            case 't':
                check_config();
                printf("Configuration file is proper.\n");
                hash_destroy(table);
                return (0);
                break;

            case 'v':
                printf("email - By: Dean Jones. Email: dj@cleancode.org.\n"
                       "Version %s - Date %s \n", EMAIL_VERSION, COMPILE_DATE);
                exit(EXIT_SUCCESS);
                break;

            case 'b':
                Mopts.blank = 1;
                break;

            case 'u':
                set_conf_value("SMTP_AUTH_USER", optarg);
                break;

            case 'i':
                set_conf_value("SMTP_AUTH_PASS", optarg);
                break;

            case 'm':
                set_conf_value("SMTP_AUTH", optarg);
                break;

            case 'g':
                set_conf_value("GPG_PASS", optarg);
                break;

            case 'H':
                explode_to_list(optarg, ",", &Mopts.headers);
                break;

            case '?':
                usage();
                break;

            case 1:
                Mopts.html = 1;
                break;

            case 2:
                Mopts.sign = 1;
                break;

            case 3:
                cc_string = optarg;
                break;

            case 4:
                bcc_string = optarg;
                break;

            default:
                /* Print an error message here  */
                usage();
                break;
        }
    }

    /* first let's check to make sure they specified some recipients */
    if (optind == argc)
        usage();

    configure();

    /* set to addresses if argc is > 1 */
    if (!(Mopts.to = getnames(argv[optind]))) {
        fatal("You must specify at least one recipient!\n");
        proper_exit(ERROR);
    }

    /* Set CC and BCC addresses */
    if (cc_string)
        Mopts.cc = getnames(cc_string);

    if (bcc_string)
        Mopts.bcc = getnames(bcc_string);

    signal(SIGTERM, proper_exit);
    signal(SIGINT, proper_exit);
    signal(SIGPIPE, proper_exit);
    signal(SIGHUP, proper_exit);
    signal(SIGQUIT, proper_exit);

    create_mail();
    proper_exit(0);

    /* We never get here, but gcc will whine if i don't return something */
    return (0);
}

/**
 * Usage prints helpful usage information,
**/

void
usage(void)
{
    fprintf(stderr,
            "Options information is as follows\n"
            "email [options] recipient1,recipient2,...\n\n"
            "    -h, -help module          Print this message or specify one of the "
            "below options\n"
            "    -q, -quiet                Suppress displayed messages (Good for cron)\n"
            "    -f, -from-addr            Senders mail address\n"
            "    -n, -from-name            Senders name\n"
            "    -b, -blank-mail           Allows you to send a blank email\n"
            "    -e, -encrypt              Encrypt the e-mail for first recipient before "
            "sending\n"
            "    -s, -subject subject      Subject of message\n"
            "    -r, -smtp-server server   Specify a temporary SMTP server for sending\n"
            "    -p, -smtp-port port       Specify the SMTP port to connect to\n"
            "    -a, -attach file,...      Attach N binary files and base64 encode\n"
            "    -c, -conf-file file       Path to non-default configuration file\n"
            "    -t, -check-config         Simply parse the email.conf file for errors\n"
            "        -cc email,email,...   Copy recipients\n"
            "        -bcc email,email,...  Blind Copy recipients\n"
            "        -sign                 Sign the email with GPG\n"
            "        -html                 Send message in HTML format "
            "( Make your own HTML! )\n"
            "    -m, -smtp-auth type       Set the SMTP AUTH type (plain or login)\n"
            "    -u, -smtp-user username   Specify your username for SMTP AUTH\n"
            "    -i, -smtp-pass password   Specify your password for SMTP AUTH\n"
            "    -g, -gpg-pass             Specify your password for GPG\n"
            "    -H, -header string        Add header (can be used multiple times)\n"
            "        -high-priority        Send the email with high priority\n");

    exit(EXIT_SUCCESS);
}

/**
 * ModuleUsage will take an argument for the specified 
 * module and print out help information on the topic.  
 * information is stored in a written file in the location 
 * in Macro EMAIL_DIR. and also specified with EMAIL_HELP_FILE
**/

static void
module_usage(const char *module)
{
    FILE *help;
    short found = 0;
    char *moduleptr = NULL;
    char buf[MINBUF] = { 0 };
    char help_file[MINBUF] = { 0 };

    expand_path(help_file, EMAIL_HELP_FILE, sizeof(help_file));
    if (!(help = fopen(help_file, "r"))) {
        fatal("Could not open help file: %s", help_file);
        proper_exit(ERROR);
    }


    while (fgets(buf, sizeof(buf), help) != NULL) {
        if ((buf[0] == '#') || (buf[0] == '\n'))
            continue;

        chomp(buf);
        moduleptr = strtok(buf, "|");
        if (strcasecmp(moduleptr, module) != 0) {
            while ((moduleptr = strtok(NULL, "|")) != NULL) {
                if (strcasecmp(moduleptr, module) == 0) {
                    found = 1;
                    break;
                }
            }
        }
        else
            found = 1;

        if (!found)
            continue;

        while (fgets(buf, sizeof(buf), help) != NULL) {
            if (!strcmp(buf, "EOH\n"))
                break;
            printf("%s", buf);
        }
        break;
    }

    if (feof(help)) {
        printf("There is no help in the module: %s\n", module);
        usage();
    }

    fclose(help);
    exit(0);
}
