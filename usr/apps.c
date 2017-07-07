#include "../kernel/kernel.h"
#include "../servers/ts.h"
#include "../servers/pm.h"
#include "../servers/vfs.h"
#include "../lib/mstddef.h"  /* EOF */

#include "lib/mstdlib.h"
#include <string.h>

#include "apps.h"


/*
 * getty [filename]
 *
 * opens [filename] (which should be a terminal device), then runs 'login'
 * It is normally invoked by 'init'
 */

int
getty (char** argv) {
    char* args[2];
    int i;
    if (argc(argv) < 2){
        return (-1);
    }
    args[0] = "login";
    args[1] = NULL;

    /* stdin, stdout, stderr */
    for (i = 0; i != 3; i++) {
        close(i);
        if (open(argv[1]) < 0) {
            return (-1);
        }
    }

    execv(args[0], args);
    unknown(argv, args[0]);
    return (-1);
}

/*
 * login
 *
 * Waits for an 'Enter' key, then runs shell
 * No authentication, only for legacy purposes
 * Normally invoked by 'getty'
 */

int
login (char** argv) {
    int c;
    char* args[2];
    mfprintf(1, "%s: Hit ENTER!\n", argv[0]);
    do {
        if ((c = mgetc()) == EOF) {
            return (-1);
        }
    } while((c != '\r') && (c != '\n'));

    args[0] = "sh";
    args[1] = NULL;

    execv(args[0], args);
    unknown(argv, args[0]);
    return (-1);
}

/*
 * echo
 *
 * Prints the arguments on the standard output
 */

#define ECHO_N    0x01

int
echo (char** argv) {
    int i;
    int c;
    int o = 0;
    getopt_p opt_p = initgetopt();

    while ((c = getopt(argv, "n", opt_p)) != EOF) {
        switch (c) {
          case 'n': o |= ECHO_N; break;
          case '?': pmfree(opt_p); mexit(1); break;
        }
    }
    i = opt_p->optind;
    pmfree(opt_p);

    for (; argv[i]; i++) {
        mfprintf(1, argv[i]);
        mfputc(1, ' ');
    }
    if (!(o & ECHO_N)) {
        mfputc(1, '\n');
    }
    return (0);
}

/*
 * cat
 *
 * concatenate files and print on the standard output
 */

void
docat (void) {
    int c;
    while((c = mgetc()) != EOF) {mfputc(1, c);}
}

int
cat (char** argv) {
    int i;
    if (!argv[1]) {
        docat();
        return (0);
    }
    for (i=1; argv[i]; i++) {
        close(0); /* stdin */
        if(open(argv[i]) < 0){
            unknown(argv, argv[i]);
            return (-1);
        }
        docat();
    }
    return (0);
}

/*
 * cap
 *
 * concatenate files and print on the standard output
 * changes lower-case letters to capitals
 */

void
docap (void) {
    int c;
    while((c = mgetc()) != EOF) {
        if (c >= 'a' && c <= 'z') {
            c = c - ('a' - 'A');
        }
        mfputc(1, c);
    }
}

int
cap (char** argv) {
    int i;
    if (!argv[1]) {
        docap();
        return (0);
    }
    for (i=1; argv[i]; i++) {
        close(0); /* stdin */
        if(open(argv[i]) < 0){
            unknown(argv, argv[i]);
            return (-1);
        }
        docap();
    }
    return (0);
}

/*
 * sleep
 *
 * delay for a specified amount of time (seconds)
 */

int
sleep (char** argv) {
    if (argc(argv) < 2) {
        noargs(argv);
        return (-1);
    }
    delay(atoi(argv[1]) * 30);
    return (0);
}

/*
 * xargs
 *
 * build and execute command lines from standard input
 */

int
xargs (char** argv) {
    char* buf;
    char* args[16]; // max 16 elements
    int i;
    int c;
    int ac;
    char* lastwrd; /* NULL: space, NOTNULL: word */

    if (argc(argv) < 2) {
        noargs(argv);
        return (-1);
    }
    memset(args, 0x00, sizeof(args));
    buf = pmmalloc(256);    // max 256 bytes

    for (ac = 0; argv[ac + 1]; ac++) {
        args[ac] = argv[ac + 1];
    }
    i = 0;
    lastwrd = NULL;
    do {
        c = mgetc();
        switch (c) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case EOF:
            if(lastwrd){
                args[ac++] = lastwrd;
                lastwrd = NULL;
                buf[i++] = '\0';
            }
            break;
          default:
            if(!lastwrd){
                lastwrd = &(buf[i]);
            }
            buf[i++] = c;
            break;
        }
    } while (c != EOF);

    execv(args[0], args);
    unknown(argv, argv[1]);
    pmfree(buf);
    return (-1);
}

/*
 * repeat
 *
 * repeat a command for a specified times
 */

int
do_repeat (char** argv) {
    execv(argv[2], &(argv[2]));
    unknown(argv, argv[2]);
    return (-1);
}

int
repeat (char** argv) {
    int n;
    int i;
    if (argc(argv) < 3) {
        noargs(argv);
        return (-1);
    }
    n = atoi(argv[1]);
    for (i = 0; i != n; i++) {
        spawntask(do_repeat, DEFAULT_STACK_SIZE, argv);
        wait(NULL);
    }
    return (0);
}

/*
 * uptime
 *
 * tell how long the system has been running
 */

int
pr_uptime (char** argv UNUSED) {
    time_t ut;
    int c, opt = 0;
    getopt_p opt_p = initgetopt();

    while ((c = getopt(argv, "s", opt_p)) != EOF) {
        switch (c) {
          case 's': opt = 1; break;
          case '?': pmfree(opt_p); mexit(1); break;
        }
    }
    pmfree(opt_p);

    getuptime(&ut);
    if (opt) {
        mfputu(1, ut.sec + ut.min * 60 + ut.hour * 3600);
    } else {
        mfprintf(1, "%d:%d:%d", ut.hour, ut.min, ut.sec);
    }
    mfputc(1, '\n');
    return (0);
}

/*
 * stat
 *
 * display file information
 */

int
f_stat (char** argv) {
    struct stat st;
    int i;
    if (argc(argv) < 2) {
        noargs(argv);
        return (-1);
    }
    for (i=1; argv[i]; i++) {
        if (fstat(argv[i], &st) < 0) {
            unknown(argv, argv[i]);
            return (-1);
        }
        mfprintf(1, "%s:\n ino:%x\n dev:%x\n size:%d\n", argv[i],
                 st.ino,
                 st.dev,
                 st.size);
    }
    return (0);
}


/*
 * grep
 *
 * print lines matching a pattern
 */


#define GREP_INV    0x01

int
dogrep (char* regexp, int opt) {
    char* line;
    int c = 0, pos, rc = 1;
    while (c != EOF) {
        line = pmmalloc(64);
        pos = 0;
        while ((c = mgetc()) != EOF) {
            line[pos] = c;
            if (c == '\n') {
                line[pos] = '\0';
                break;
            }
            pos++;
        }
        if (pos) { /* otherwise it's just an eof */
            pos = (int)strstr(line, regexp);
            rc = pos ? 0 : rc;
            if (opt & GREP_INV ? !pos : pos) {
                mfprintf(1, "%s\n", line);
            }
        }
        pmfree(line);
    }
    return (rc);
}


int
grep (char** argv) {
    int i;
    int c, opt = 0;
    char* regexp;
    getopt_p opt_p = initgetopt();

    while ((c = getopt(argv, "v", opt_p)) != EOF) {
        switch (c) {
          case 'v': opt |= GREP_INV; break;
          case '?': pmfree(opt_p); mexit(1); break;
        }
    }
    i = opt_p->optind;
    pmfree(opt_p);
    if (!argv[i]) {
        noargs(argv);
        return (-1);
    }
    regexp = argv[i++];
    if (!argv[i]) {
        return(dogrep(regexp, opt));
    }
    c = 1;
    for (; argv[i]; i++) {
        close(0); /* stdin */
        if (open(argv[i]) < 0) {
            unknown(argv, argv[i]);
            return (-1);
        }
        if (!dogrep(regexp, opt)) {
            c = 0;
        }
    }
    return (c);
}

/*
 * mknod
 *
 * make files on the file system
 *
 *          [dev num]   0:pipe, 1:usart, 2:memfile
 *
 *  $ mknod 0
 */

int
f_mknod (char** argv) {

    int     inum;

    if (argc(argv) < 2) {
        noargs(argv);
        return (-1);
    }

    inum = mknod(atoi(argv[1]), NULL);
    mfprintf(1, " %d/%d\n", atoi(argv[1]), inum);
    return (0);
}

