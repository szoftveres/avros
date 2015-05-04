#include <string.h>
#include "kernel.h"
#include "apps.h"
#include "misc.h"
#include "sema.h"
#include "timer.h"
#include "pm.h"
#include "dm.h"  /* EOF */
#include "es.h"

/*
================================================================================
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
    for (i = 0; i < 3; i++) {
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
================================================================================
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
================================================================================
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
================================================================================
*/

void
docat (void) {
    int c;
    while((c = mgetc()) != EOF) {mfputc(1, c);}
}

/**
*/

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
================================================================================
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
================================================================================
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
================================================================================
*/

int
do_at (char** argv) {
    delay(atoi(argv[1]) * 30);
    execv(argv[2], &(argv[2]));
    unknown(argv, argv[2]);
    return (-1);
}

int
at (char** argv) {
    if (argc(argv) < 3) {
        noargs(argv);
        return (-1);
    }
    spawntask(do_at, DEFAULT_STACK_SIZE, argv);
    return (0);
}


/*
================================================================================
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
================================================================================
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
================================================================================
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
        mfprintf(1, "%s:\n ino:%x\n dev:%x\n size:%d\n mode:%d\n", argv[i],
                 st.ino,
                 st.dev,
                 st.size,
                 st.mode);
    }
    return (0);
}

/*
================================================================================
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

/**
*/

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


