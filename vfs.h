#ifndef _VFS_H_
#define _VFS_H_

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
    VFS_NONE,
    VFS_INTERRUPT,
    VFS_DONTREPLY,
    VFS_REPEAT,

    VFS_STAT,

    VFS_MKDEV,

    VFS_MKNOD,
    VFS_RMNOD,

    VFS_DUP,


    VFS_IGET,
    VFS_IPUT,

    VFS_LINK,
    VFS_UNLINK,
    

    VFS_PIPE,

    VFS_OPEN,
    VFS_CLOSE,
    VFS_CREAT,

    VFS_READC,                  /* char read */
    VFS_WRITEC,                 /* char write */

    VFS_ADDTASK,                /* Add taks */
    VFS_DELTASK                 /* Del task */
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
        void(*driver)(void* args);        /* driver */
        void* args;                         /* driver */
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
    int         ino;
    mode_t      mode;
} iget_t;

typedef union iput_u {
    int         ino;
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

typedef struct vfsmsg_s {
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
} vfsmsg_t;

/*
 *
 */


void vfs (void* args);

pid_t setvfspid (pid_t pid);









pid_t vfs_cratetask (pid_t pid, pid_t parent);
void vfs_deletetask (pid_t pid);
int mkdev (void(*p)(void* args), void* args);
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