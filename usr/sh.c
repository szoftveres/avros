#include "../kernel/kernel.h"
#include "../servers/vfs.h"
#include "../servers/pm.h"

#include "../lib/mstddef.h"
#include "lib/mstdlib.h"
#include <string.h>

#include "sh.h"

/**
*/

#define MAX_CMDLINE 128
#define MAX_JOBLEN  32
#define MAX_ARGS    8


#define CMD_WSPACE  1
#define CMD_LETTER  2
#define SIN         3
#define SOUT        4


static void
parse_args (char* t,
            char** stin,
            char** stout,
            char** args) {
    int i;
    int state = CMD_WSPACE;

    for (i = 0; i < (MAX_ARGS + 1); i++) {
        args[i] = NULL;
    }

    i=0;
    while (*t) {
        switch (*t) {
          case '<': state = SIN; *t='\0'; break; /* no more args */
          case '>': state = SOUT; *t='\0'; break; /* no more args */
          case ' ':
          case '\t':
            if(args[i] && state == CMD_LETTER){
                i++;
                state = CMD_WSPACE;
            }
            /* fallthrough */
          case '\n':
          case '\r':
            *t='\0';
            break;
          default:
            if (state == CMD_WSPACE) {
                args[i] = t;
            } else if ((state == SIN) && stin) {
                *stin = t;
            } else if ((state == SOUT) && stout) {
                *stout = t;
            }
            state = CMD_LETTER;
            break;            
        }
        t++;
    }
    return;
}




static int
exec_job (char** argv) {
    int i;
    char* cmd;
    char* stin = NULL;
    char* stout = NULL;
    char** args = pmmalloc(sizeof(char*) * (MAX_ARGS + 1));
    ASSERT(args);
    /* 
     * This is mandatory because shell keeps the other
     * end of the pipe and this process inherits it
     */
    for (i = STDMAX; i != MAX_FD; i++) {
        close(i);
    }

    cmd = pmmalloc(MAX_JOBLEN);
    ASSERT(cmd);
    receive(TASK_ANY, cmd, MAX_JOBLEN);  

    parse_args(cmd, &stin, &stout, args);

    if (!args[0]) {
        pmfree(cmd);
        pmfree(args);
        return 0; /* Empty line */
    }

    if (stin) {
        close(STDIN);
        if (open(stin) < 0) {
            unknown(argv, stin);
            pmfree(cmd);
            pmfree(args);
            return (-1);
        }
    }

    if (stout) {
        close(STDOUT);
        if (open(stout) < 0) {
            unknown(argv, stout);
            pmfree(cmd);
            pmfree(args);
            return (-1);
        }
    }
    execv(args[0], args);
    unknown(argv, args[0]);
    pmfree(cmd);
    pmfree(args);
    return (-1);
}


/**
*/

static int
builtin (char* cmd) {
    char* line = pmmalloc(MAX_JOBLEN);;
    char** args = pmmalloc(sizeof(char*) * (MAX_ARGS + 1));
    int rc = 0;

    ASSERT(line);
    ASSERT(args);
    strcpy(line, cmd);
    parse_args(line, NULL, NULL, args);
    
    if (!strcmp(args[0], "exit")) {
        rc = 1;
        mexit(args[1]?atoi(args[1]):0);
    }

    if (!strcmp(args[0], "cd")) {
        rc = 1;
        if (!args[1]) {
            noargs(args);
        } else {
            mfprintf(STDOUT,"cd %s\n", args[1]);
            //chdir(args[1]);
        }
    }

    pmfree(args);
    pmfree(line);
    return (rc);
}


/**
  This task will launch the exec process and exits immediately
  thus the shell won't have any zombies
*/

static int
launch_job (char** argv) {
    char *cmd = (char*) pmmalloc(MAX_JOBLEN);
    ASSERT(cmd);
    receive(TASK_ANY, cmd, MAX_JOBLEN);
    send(spawntask(exec_job, DEFAULT_STACK_SIZE + 64, argv), cmd);
    pmfree(cmd);
    return (0);
}


static int
execute (char interactive, char direct, char* line, char** argv) {
    char* job;
    int code;
    if (direct) {
        if (builtin(line)) {
            return 0;
        }
    }
    job = (char*) pmmalloc(MAX_JOBLEN); /* buffer for one cmd */
    ASSERT(job);
    strcpy(job, line);          /* filling with one section */
    send(spawntask(direct ? exec_job: launch_job, DEFAULT_STACK_SIZE+64, argv), job);
    pmfree(job);                /* deleting buffer */
    wait(&code);
    if (direct && interactive) {
        mfprintf(STDERR, " (%d)\n", code);
    }
    return (code);
}

/**
*/

static int
getcmd (int fin, char* cmd, int len) {
    int  c;
    int  i = 0;
    while (1) {
        c = readc(fin);
        if (c == EOF) {
            return c;
        }
        if (i >= (len-1)) {
            i--;
        }
        if ((c == '\r') || (c == '\n')) {
            break;
        }
        cmd[i++] = c;
        i -= (i == len-1) ? 1 : 0;
    }
    cmd[i] = '\0';
    return 0;
}

/**
*/

#define JOB_INIT                        \
    close(STDIN);                       \
    dup(3);                             \
    close(STDOUT);                      \
    dup(4);                             \
    nextin = 3;                         \


#define JOB_PIPE                        \
    close(STDIN);                       \
    dup(nextin);                        \
    close(5);                           \
    pipe(pip);                          \
    close(STDOUT);                      \
    dup(pip[1]);                        \
    close(pip[1]);                      \
    nextin = pip[0];                    \


#define JOB_NOPIPE                      \
    close(STDIN);                       \
    dup(nextin);                        \
    close(5);                           \
    close(STDOUT);                      \
    dup(4);                             \



/**
*/

int
sh (char** argv) {
    int code;
    int pip[2];
    int nextin;
    char *cmdline, *p, *jobstart;
    int fin = STDIN;

    nextin = dup(STDIN);    /* 3 = stdin  */
    dup(STDOUT);            /* 4 = stdout */
    JOB_INIT
    dup(STDIN);             /* 5 needed by pipe */

    if (argv[1]) {
        if ((fin = open(argv[1])) < 0) {
            unknown(argv, argv[1]);
            return 1;
        }
        ASSERT(fin == 6);
    }
    close(5);

    while (1) {
        if (fin == 0) { /* interactive */
            mfprintf(STDOUT, "$ "); /* prompt */
        }
        cmdline = (char*) pmmalloc(MAX_CMDLINE);
        ASSERT(cmdline);
        if (getcmd(fin, cmdline, MAX_CMDLINE) == EOF) {
            if (fin == 0) {
                mfprintf(STDERR, "\nExit\n");
            }
            mexit(0);
        }

        p = cmdline;
        jobstart = cmdline;
        /* Go through command line until term null */
        while (*p) {
            switch (*p) {
              case '\\': /* ignore next char */
                memmove(p, p+1, strlen(p)+1); /* move string left by one */
                break;
              case '#': /* comment */
                *p = '\0';
                continue;
              case '|': /* pipe for future */
                JOB_PIPE
                *p++ = '\0';
                execute((fin == STDIN), 0, jobstart, argv);
                jobstart = p; /**/
                continue; /* pointer has been increased already */
              case '&': /* job section boundary */
                JOB_NOPIPE
                *p++ = '\0';
                execute((fin == STDIN), 0, jobstart, argv);
                jobstart = p; /**/
                JOB_INIT
                continue; /* pointer has been increased already */
              case ';': /* command sequence boundary */
                JOB_NOPIPE
                *p++ = '\0';
                code = execute((fin == STDIN), 1, jobstart, argv);
                jobstart = p; /**/
                JOB_INIT
                continue; /* pointer has been increased already */
            }
            p++;
        }
        /* launching last job directly and waiting for it */
        JOB_NOPIPE
        code = execute((fin == STDIN), 1, jobstart, argv);
        code = code;
        pmfree(cmdline);
        JOB_INIT
    }
}


/*


exec                builtin  [arg_list]
                    executable  [arg_list]

cmd                 exec  [ < in_fname ]  [ > out_fname ]
                    ( cmd_grp )

cmd_grp             cmd  [ |  cmd_grp ] [&]

job                 cmd_grp [; [ job ] ] \n

jobs                job [jobs]


*/

