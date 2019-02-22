#ifndef _VFS_H_
#define _VFS_H_

#include "../lib/queue.h"
#include "../lib/mstddef.h"

enum {
    /* Answer types */
    VFS_HOLD,   /* Driver unblocks VFS but the user task remains blocked */
    VFS_REPEAT, /* Driver indicates that more than one user tasks may be unblocked */
    VFS_FINAL,  /* End of transaction, unblocking VFS and/or user task */

    /* Query types */
    VFS_RD_INTERRUPT,
    VFS_WR_INTERRUPT,

    VFS_INODE_GRAB,
    VFS_INODE_RELEASE,
    VFS_LINK,
    VFS_UNLINK,

    VFS_MKNOD,
    VFS_MKDEV,


    VFS_GET_DIRENTRY,
    VFS_DEBUG,


    VFS_STAT,
    VFS_DUP,
    VFS_PIPE,
    VFS_OPEN,
    VFS_CLOSE,
    VFS_CREAT,
    VFS_READC,
    VFS_WRITEC,
    VFS_ADDTASK,
    VFS_DELTASK
};

/*
 *
 */

struct stat {
    int     dev;
    int     ino;
    int     size;
};

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

typedef union stat_u {
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
    int             data;
} interrupt_t;

/*
 * MKNOD
 */

typedef struct mknod_s {
    union {
        int                 dev;      /* dev in devtab */
        int                 ino;        /* ino number */
    };
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
typedef struct iget_s {
    int         ino;
} iget_t;

typedef struct link_s {
    int         dev;
    int         ino;
    char*       name;
} link_t;

/*
 * MESSAGE
 */

typedef struct vfsmsg_s {
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
        link_t          link;
    };
} vfsmsg_t;

typedef struct vfsmsg_container_s {
    QUEUE_HEADER;
    vfsmsg_t        msg;
} vfsmsg_container_t;


/*
 *
 */

void vfs (void* args);

void vfs_rd_interrupt (pid_t driver);
void vfs_wr_interrupt (pid_t driver);

pid_t setvfspid (pid_t pid);

pid_t vfs_createtask (pid_t pid, pid_t parent);
void vfs_deletetask (pid_t pid);
int mkdev (void(*p)(void* args), void* args);
int mknod (int dev, char* name);
int pipe(int pipefd[2]);
int open (char *name);
int openi (int dev, int inum);
int dup (int fd);
int fstat (char *name, struct stat *st_stat);
void close (int fd);
int readc (int fd);
int writec (int fd, int c);

int vfs_debugn (int reset);

#endif
