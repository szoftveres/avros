#ifndef _DM_H_
#define _DM_H_

#include "queue.h"

typedef enum mode_s {
    S_IFREG,
    S_IFCHR,
    S_IFIFO
} mode_t;


#define MAX_FD          (8)

enum {   
    DM_NONE,
    DM_INTERRUPT,
    DM_DONTREPLY,

    DM_STAT,

    DM_MKDEV,
    DM_MKDEV_ANS,
    DM_RMDEV,
    DM_RMDEV_ANS,

    DM_MKNOD,
    DM_MKNOD_ANS,
    DM_RMNOD,
    DM_RMNOD_ANS,

    DM_DUP,
    DM_DUP_ANS,


    DM_PIPE,
    DM_PIPE_ANS,

    DM_OPEN,
    DM_OPEN_ANS,
    DM_CLOSE,
    DM_CLOSE_ANS,

    DM_READC,                    /* char read */
    DM_READC_ANS,
    DM_WRITEC,                    /* char write */
    DM_WRITEC_ANS,

    DM_ADDTASK,                /* Add taks */
    DM_DELTASK                 /* Del task */
};

/*
 *
 */

struct stat {
    int     dev;
    int     ino;
    int     size;
    mode_t  mode;
};


#define EOF	(-1)

/*
 * ADD/DEL CLIENT
 */

typedef struct adddel_s {
    pid_t           pid;            /* pid */
    pid_t           parent;         /* parent */
} adddel_t;


/*
 * CHAR READ/WRITE
 */

typedef struct rwc_s {
    int             fd;
    int             pos;
    int             data;       /* data */
} rwc_t;

/*
 * OPEN CLOSE
 */

typedef struct openclose_s {
    union {
        char*           name;         /* fname */
        int             fd;         /* fd */
    };
} openclose_t;

/*
 * STAT
 */

typedef struct stat_ask_s {
    char*           name;         /* fname */
} stat_ask_t;

typedef struct stat_ans_s {
    int             code;
    struct stat     st_stat;
} stat_ans_t;

typedef struct stat_s {
    stat_ask_t      ask;
    stat_ans_t      ans;
} stat_t;


/*
 * MKDEV
 */

typedef struct mkdev_ask_s {
    void(*driver)(void);        /* driver */
} mkdev_ask_t;

typedef struct mkdev_ans_s {
    int             id;         /* devtab id */
} mkdev_ans_t;

typedef union mkdev_u {
    mkdev_ask_t     ask;        /* ask */
    mkdev_ans_t     ans;        /* answer */
} mkdev_t;


/*
 *
 */

typedef struct interrupt_s {
    int             cmd;
    int             data;
} interrupt_t;

/*
 * MKNOD
 */

typedef struct mknod_s {
    union {
        int                 id;      /* dev in devtab */
        int                 ino;        /* ino number */ 
    };
    mode_t              mode;
    char*               name;       /* name */
} mknod_t;

/*
 * PIPE
 */

typedef struct pipe_s {
    int             fdi;
    int             fdo;
    int             result;
} pipe_t;

/*
 * DUP
 */

typedef struct dup_s {
    int             fd;
} dup_t;


/*
 * PARAMETERS
 */

typedef union param_u {
    interrupt_t     interrupt;
    stat_t          stat;       /* stat */
    openclose_t     openclose;  /* open close*/
    rwc_t           rwc;        /* character read/write */
    mkdev_t         mkdev;
    mknod_t         mknod;
    dup_t           dup;
    pipe_t          pipe;
    adddel_t        adddel;        /* Client add del*/
} param_t;

/*
 * MESSAGE
 */

typedef struct dmmsg_s {
    int             cmd;        /* command */
    pid_t           client;     /* client (client who requests IO) */
    param_t         param;      /* parameter */
} dmmsg_t;

/*
 *
 */


typedef struct msgq_s {
    QUEUE_HEADER;
    dmmsg_t msg;  
} msgq_t;



void dm (void);

pid_t setdmpid (pid_t pid);









pid_t dm_addtask (pid_t pid, pid_t parent);
void dm_deletetask (pid_t pid);
int mkdev (void(*p)(void));
int mknod (int dev, char* name, mode_t mode);
int pipe(int pipefd[2]);
int open (char *name);
int dup (int fd);
int fstat (char *name, struct stat *st_stat);
void close (int fd);
int readc (int fd);
int writec (int fd, int c);



void
dputc (int data);
void
dputs (const char* str);
void
dputu (unsigned int num);




#endif
