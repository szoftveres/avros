#ifndef _DM_H_
#define _DM_H_

#include "queue.h"



typedef enum mode_s {
    S_IFREG,    /* Regular */
    S_IFCHR,    /* Character Special */
    S_IFIFO,    /* FIFO Special */
//    S_IFDIR,    /* Directory */
//    S_IFMNT,    /* Mount point */
} mode_t;


#define MAX_FD          (8)

enum {   
    DM_NONE,
    DM_INTERRUPT,
    DM_DONTREPLY,

    DM_STAT,

    DM_MKDEV,

    DM_MKNOD,
    DM_RMNOD,

    DM_DUP,


    DM_IGET,
    DM_IPUT,

    DM_LINK,
    DM_UNLINK,
    

    DM_PIPE,

    DM_OPEN,
    DM_CLOSE,
    DM_CREAT,

    DM_READC,                  /* char read */
    DM_WRITEC,                 /* char write */

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
    union {
        int             fd;
        int             ino;
    };
    union {
        int             pos;
        int             bnum;
    };
    int             data;       /* data */
} rwc_t;

/*
 * OPEN CLOSE
 */

typedef struct openclose_s {
    union {
        union {
            char*           name;         /* fname */
            struct {
                int     dev;
                int     ino;
            };
        };
        int             fd;         /* fd */
    };
} openclose_t;

/*
 * STAT
 */

typedef struct stat_s {
    struct {
        char*           name;         /* fname */
    } ask;
    struct {
        int             code;
        struct stat     st_stat;
    } ans;
} stat_t;


/*
 * MKDEV
 */

typedef union mkdev_u {
    struct {
        void(*driver)(void);        /* driver */
    } ask;
    struct {
        int             id;         /* devtab id */
    } ans;
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
 *
 */
typedef union iget_u {
    struct {
        int     ino;
    } ask;
    struct {
        mode_t     mode;
    } ans;
} iget_t;

typedef union iput_u {
    struct {
        int     ino;
    } ask;
} iput_t;


typedef union link_u {
    struct {
        int     ino;
    } ask;
} link_t;

typedef union unlink_u {
    struct {
        int     ino;
    } ask;
} unlink_t;

/*
 * MESSAGE
 */

typedef struct dmmsg_s {
    QUEUE_HEADER;
    int             cmd;        /* command */
    pid_t           client;     /* client (client who requests IO) */
    union {
        interrupt_t     interrupt;
        stat_t          stat;       /* stat */
        openclose_t     openclose;  /* open close*/
        rwc_t           rw;         /* character read/write */
        mkdev_t         mkdev;
        mknod_t         mknod;
        dup_t           dup;
        pipe_t          pipe;
        adddel_t        adddel;        /* Client add del*/
        iget_t          iget;
        iput_t          iput;
        link_t          link;
        unlink_t        unlink;
    };
} dmmsg_t;

/*
 *
 */


void dm (void);

pid_t setdmpid (pid_t pid);









pid_t dm_addtask (pid_t pid, pid_t parent);
void dm_deletetask (pid_t pid);
int mkdev (void(*p)(void));
int mknod (int dev, char* name, mode_t mode);
int pipe(int pipefd[2]);
int open (char *name);
int openi (int dev, int inum);
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
