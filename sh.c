#include "kernel.h"

#include "sh.h"
#include "misc.h"
#include "apps.h"
#include "dm.h"
#include "pm.h"

#include <string.h>

/**
*/

#define MAX_CMDLINE 128
#define MAX_JOBLEN  32
#define MAX_ARGS    8


#define CMD_WSPACE  1
#define CMD_LETTER  2
#define SIN         3
#define SOUT        4


static int
exec_job (char** argv) {
    int i;
    int state = CMD_WSPACE;
    char* cmd = pmmalloc(MAX_JOBLEN);
    char* stin = NULL;
    char* stout = NULL;
    char** args = pmmalloc(sizeof(char*) * (MAX_ARGS + 1));

    /* 
     * This is mandatory because shell keeps the other
     * end of the pipe and this process inherits it
     */
    for (i = 3; i < MAX_FD; i++) {
        close(i);
    }

    char* t = cmd;
    for (i = 0; i < (MAX_ARGS + 1); i++) {
        args[i] = NULL;
    }   
    receive(TASK_ANY, cmd, MAX_JOBLEN);           
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
            } else if (state == SIN) {
                stin = t;
            } else if (state == SOUT) {
                stout = t;
            }
            state = CMD_LETTER;
            break;            
        }
        t++;
    }

    if (!args[0]) {
        pmfree(cmd);
        pmfree(args);
        return 0; /* Empty line */
    }

    if (stin) {
        close(0);
        if (open(stin) < 0) {
            unknown(argv, stin);
            pmfree(cmd);
            pmfree(args);
            return (-1);
        }
    }

    if (stout) {
        close(1);
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

static void
getcmd (char* cmd, int len) {
    int  c;
    int  i = 0;
    while (1) {
        c = mgetc();
        if (c == EOF) {
			mfputc(1, '\n');
			mexit(0);
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
}

/**
  This task will launch the exec process and exits immediately
  thus the shell won't have any zombies
*/

static int
launch_job (char** argv) {
	char *cmd = (char*) pmmalloc(MAX_JOBLEN);
    receive(TASK_ANY, cmd, MAX_JOBLEN);
    send(spawntask(exec_job, DEFAULT_STACK_SIZE, argv), cmd);
	pmfree(cmd);
	return (0);
}

#define JOB_INIT                        \
    close(0);                           \
    dup(3);                             \
    close(1);                           \
    dup(4);                             \
    nextin = 3;                         \


#define JOB_PIPE                        \
    close(0);                           \
    dup(nextin);                        \
    close(5);                           \
    pipe(pip);                          \
    close(1);                           \
    dup(pip[1]);                        \
    close(pip[1]);                      \
    nextin = pip[0];                    \


#define JOB_NOPIPE                      \
    close(0);                           \
    dup(nextin);                        \
    close(5);                           \
    close(1);                           \
    dup(4);                             \
    


/**
*/

int
sh (char** argv) {
    int code;
    int pip[2];
    int nextin;
	char *cmdline, *job, *p, *jobstart;
    
    nextin = dup(0); /* 2 = stdin  */
    dup(1); /* 3 = stdout */
    JOB_INIT


    while (1) {
        mfprintf(1, "$ "); /* prompt */
		cmdline = (char*) pmmalloc(MAX_CMDLINE);
        getcmd(cmdline, MAX_CMDLINE);
        
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
                job = (char*) pmmalloc(MAX_JOBLEN); /* buffer for one cmd */
                strcpy(job, jobstart);					/* filling with one section */
                /* calling an intermediate process : job launcher */
                send(spawntask(launch_job, DEFAULT_STACK_SIZE, argv), job);
                pmfree(job);							/* deleting buffer */
                wait(NULL);     /* waiting for launcher to finish */
                jobstart = p; /**/
                continue; /* pointer has been increased already */
              case '&': /* job section boundary */
                JOB_NOPIPE
                *p++ = '\0'; 
                job = (char*) pmmalloc(MAX_JOBLEN); /* buffer for one cmd */
                strcpy(job, jobstart);					/* filling with one section */
                /* calling an intermediate process : job launcher */
                send(spawntask(launch_job, DEFAULT_STACK_SIZE, argv), job);
                pmfree(job);							/* deleting buffer */
                wait(NULL);     /* waiting for launcher to finish */
                jobstart = p; /**/
                JOB_INIT
                continue; /* pointer has been increased already */
              case ';': /* command sequence boundary */
                JOB_NOPIPE
                *p++ = '\0'; 
                job = (char*) pmmalloc(MAX_JOBLEN); /* buffer for one cmd */
                strcpy(job, jobstart);					/* filling with one section */
                send(spawntask(exec_job, DEFAULT_STACK_SIZE, argv), job);
		        pmfree(job);							/* deleting buffer */
                wait(NULL);     /* waiting for launcher to finish */
                jobstart = p; /**/
                JOB_INIT
                continue; /* pointer has been increased already */
			}
			p++;
		}		
		/* launching last job directly and waiting for it */
        JOB_NOPIPE
		job = (char*) pmmalloc(MAX_JOBLEN); /* buffer for one cmd */
		strcpy(job, jobstart);					/* filling with one section */
        send(spawntask(exec_job, DEFAULT_STACK_SIZE, argv), job);
		pmfree(job);							/* deleting buffer */
        wait(&code);
		pmfree(cmdline);
        JOB_INIT
    }
}

