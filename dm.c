#include "kernel.h"
#include "dm.h"
#include "queue.h"

/*
 * DEVTAB
 */

#define MAX_DEV         (6) /* u1, u2, null, pipe, eeprom, ramdisk */
static pid_t            devtab[MAX_DEV];

/*
 * INODE
 */

typedef struct inode_s {
    int                 dev;
    char                refcnt;
    char                links;
    mode_t              mode;
    int                 size;
    q_head_t            msgs;
} inode_t;

#define MAX_I           (12)
static inode_t          itab[MAX_I];

/*
 * DIRTAB
 */
typedef struct dirtab_s {
    QUEUE_HEADER
    int                 ino;
    char*               name;
} dirtab_t;

static q_head_t         dirtab;
/*
 *  FILP
 */

typedef struct filp_s {
    int                 inode;
    char                refcnt;
    int                 pos;
} filp_t;

#define MAX_FILP        (16)
static filp_t           filp[MAX_FILP];

/*
 *  TASK
 */

typedef struct dm_task_s {
    QUEUE_HEADER
    pid_t               pid;
    int                 fd[MAX_FD];
} dm_task_t;

static q_head_t         dm_task_q;

/*
 *
 */

static void
dm_init (void) {
    memset(filp, 0, (sizeof(filp)));
    memset(devtab, 0, (sizeof(devtab)));
    memset(itab, 0, (sizeof(itab)));
    q_init(&dirtab);
    q_init(&dm_task_q); 
}

/*
 *
 */

static int
find_empty_devtab (void) {
    int i;
    for (i=0; i < MAX_DEV; i++) {
        if (!devtab[i]) {
            return i;
        }
    }
    return (-1);
}

/*
 *
 */

static int
find_empty_itab (void) {
    int i;
    for (i=0; i < MAX_I; i++) {
        if (!itab[i].refcnt && !itab[i].links) {
            return i; 
        }
    }
    return (-1);
}

/*
 *
 */

static int
find_empty_filp (void) {
    int i;
    for (i=0; i < MAX_FILP; i++) {
        if (!filp[i].refcnt) {
            return i;
        }
    }
    return (-1);
}

/*
 *
 */

static int
find_empty_fd (dm_task_t *client) {
    int i;
    for (i=0; i < MAX_FD; i++) {
        if (client->fd[i] < 0) {
            return i;
        }
    }
    return (-1);
}

/*
 *
 */

static dirtab_t*
dm_findbyname (char* name) {
    dirtab_t *it = (dirtab_t*)Q_FIRST(dirtab);
    while (it) {
        if (!strcmp(it->name,name)) {
            break;
        }
        it = (dirtab_t*)Q_NEXT(it);
    }
    return (it);
}

/*
 * ====================================================
 */


static void
do_dup (dm_task_t *client, dmmsg_t *msg) {
    int fp;
    int fd;
    if (msg->param.dup.fd < 0) {
        msg->param.dup.fd = -1;  /* Nothing to dup */
        return;
    }
    fp = client->fd[msg->param.dup.fd];
    if (fp < 0) {
        msg->param.dup.fd = -1;  /* Nothing to dup */
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->param.dup.fd = -1;  /* No free fd */
        return;
    }
    client->fd[fd] = fp;
    filp[fp].refcnt++;
    msg->param.dup.fd = fd;
    return;
}

/*
 *
 */

static void
do_mkdev (dmmsg_t *msg) {
    int i;
    pid_t pid;

    i = find_empty_devtab();
    if (i < 0) {
        msg->param.mkdev.ans.id = -1;
        return;
    }
    pid = addtask(TASK_PRIO_HIGH);
    launchtask(pid, msg->param.mkdev.ask.driver, DEFAULT_STACK_SIZE);
    devtab[i] = pid;

    msg->cmd = DM_MKDEV;
    sendrec(pid, msg, sizeof(dmmsg_t));
    msg->param.mkdev.ans.id = i;
    return;
}

/*
 *
 */

static void
do_mknod (dmmsg_t *msg) {
    int ino;
    dirtab_t *dirent;
    ino = find_empty_itab();            
    if (ino < 0) {
        msg->param.mknod.ino = -1;
        return;
    }
    dirent = (dirtab_t*)kmalloc(sizeof(dirtab_t));
    if (!dirent) {
        msg->param.mknod.ino = -1;
        return;
    }
    dirent->name = kmalloc(strlen(msg->param.mknod.name)+1);
    if (!dirent->name) {
        kfree(dirent);
        msg->param.mknod.ino = -1;
        return;
    }
    Q_END(&dirtab, dirent);
    strcpy(dirent->name, msg->param.mknod.name);
    itab[ino].dev = msg->param.mknod.id;
    itab[ino].mode = msg->param.mknod.mode;
    itab[ino].size = 0;
    itab[ino].links++; /* hard link count */
    q_init(&(itab[ino].msgs));
    dirent->ino = ino;
    msg->param.mknod.ino = ino;
    return;
}


/* 
 *
 */

static void
do_stat (dmmsg_t *msg) {
    dirtab_t* dirent = dm_findbyname(msg->param.stat.ask.name);
    if (!dirent) { /* name not found, error */ 
        msg->param.stat.ans.code = -1;
        return;
    }
    msg->param.stat.ans.st_stat.ino = dirent->ino;
    msg->param.stat.ans.st_stat.dev = itab[dirent->ino].dev;
    msg->param.stat.ans.st_stat.size = itab[dirent->ino].size;
    msg->param.stat.ans.st_stat.mode = itab[dirent->ino].mode;
    msg->param.stat.ans.code = 0;
    return;
}

/*
 *
 */

static void
do_pipe (dm_task_t *client, dmmsg_t *msg) {
    int fd;
    int fp;
    int ino;
    ino = find_empty_itab();            
    if (ino < 0) {
        msg->param.pipe.result = -1;
        return;
    }
    itab[ino].mode = S_IFIFO;
    q_init(&(itab[ino].msgs));

    /* input */
    fp = find_empty_filp();
    if (fp < 0) {
        msg->param.pipe.result = -1;
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->param.pipe.result = -1;
        return;
    }
    client->fd[fd] = fp;
    filp[fp].inode = ino;
    filp[fp].refcnt++;
    itab[ino].refcnt++; 
    msg->param.pipe.fdi = fd;

    /* output */
    fp = find_empty_filp();
    if (fp < 0) {
        msg->param.pipe.result = -1;
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->param.pipe.result = -1;
        return;
    }
    client->fd[fd] = fp;
    filp[fp].inode = ino;
    filp[fp].refcnt++;
    itab[ino].refcnt++;
 
    msg->param.pipe.fdo = fd;

    msg->param.pipe.result = 0;
    return;
}

/*
 *
 */

static void
do_open (dm_task_t *client, dmmsg_t *msg) {
    int fd;
    int fp;
    int ino;
    dirtab_t* dirent = dm_findbyname(msg->param.openclose.name);
    if (!dirent) { /* name not found, error */ 
        msg->param.openclose.fd = -1;
        return;
    }
    ino = dirent->ino;
    fp = find_empty_filp();
    if (fp < 0) {
        msg->param.openclose.fd = -1;
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->param.openclose.fd = -1;
        return;
    }
    client->fd[fd] = fp;
    filp[fp].pos = 0;
    filp[fp].inode = ino;
    filp[fp].refcnt++;
    itab[ino].refcnt++; 
    msg->param.openclose.fd = fd;
    return;
}

/*
 *
 */

static void
do_close (dm_task_t *client, dmmsg_t *msg) {
    int fp;
    int ino;
    int fd = msg->param.openclose.fd;
    if (fd < 0) {
        return; /* Nothing to close */
    }   
    fp = client->fd[fd];
    if (fp < 0) {
        return; /* Nothing to close */
    }
    ino = filp[fp].inode;
    client->fd[fd] = (-1);

    if ((--(filp[fp].refcnt))) {
        return; /* refcnt not zero yet */
    }
    if (itab[ino].mode == S_IFIFO) {
        /* One end of the pipe has closed,
         * hence sending EOF to waiting clients */
        while(!Q_EMPTY(itab[ino].msgs)) {
            ((dmmsg_t*)Q_FIRST(itab[ino].msgs))->param.rwc.data = EOF;
            send(((dmmsg_t*)Q_FIRST(itab[ino].msgs))->client, ((dmmsg_t*)Q_FIRST(itab[ino].msgs)));
            kfree(Q_REMV(&(itab[ino].msgs), Q_FIRST(itab[ino].msgs)));
        }
    }
    if ((--(itab[ino].refcnt)) ||
            itab[ino].links) {
        return; /* refcnt || link cnt not zero yet */
    }

    if (itab[ino].mode == S_IFIFO) {
        /* Both ends closed, remove node */
        ;
    } else {
        msg->cmd = DM_RMNOD;
        sendrec(devtab[itab[ino].dev], msg, sizeof(dmmsg_t));
    }
    return;
}

/*
 *
 */

static void
do_rw (dm_task_t *client, dmmsg_t *msg) {
    int fd = msg->param.rwc.fd;
    int ino;
    if ((fd < 0) || (client->fd[fd] < 0)) {
        msg->param.rwc.data = EOF; /* wrong fd */
        return;
    }
    ino = filp[client->fd[fd]].inode;

    switch (itab[ino].mode) {
      case S_IFREG:
        msg->param.rwc.pos = filp[client->fd[fd]].pos;
        
        switch (msg->cmd) {
          case DM_WRITEC:
            filp[client->fd[fd]].pos++; /* Assumption: no error during r/w */
            itab[ino].size = filp[client->fd[fd]].pos;
            break;
          case DM_READC:
            if (filp[client->fd[fd]].pos >= itab[ino].size) {
                msg->param.rwc.data = EOF; /* reached end of file */
                return;
            }
            filp[client->fd[fd]].pos++; /* Assumption: no error during r/w */
            break;
        }
        sendrec(devtab[itab[ino].dev], msg, sizeof(dmmsg_t));        
        break;

      case S_IFCHR:
        sendrec(devtab[itab[ino].dev], msg, sizeof(dmmsg_t));
        break;

      case S_IFIFO:
        if (Q_EMPTY(itab[ino].msgs)) {   /* Empty pipe */
            if (itab[ino].refcnt <= 1) {
                /* Other end detached, send EOF */
                msg->param.rwc.data = EOF;
            } else {
                /* FIFO empty, save request */
                dmmsg_t* newmsg = (dmmsg_t*) kmalloc(sizeof(dmmsg_t));
                memcpy(newmsg, msg, sizeof(dmmsg_t));
                Q_END(&(itab[ino].msgs), newmsg);
                msg->cmd = DM_DONTREPLY;
            }
        } else {        /* Read or Write requests in the pipe */
            if (((dmmsg_t*)Q_FIRST(itab[ino].msgs))->cmd == msg->cmd) {
                /* Same request type, save it */
                dmmsg_t* newmsg = (dmmsg_t*) kmalloc(sizeof(dmmsg_t));
                memcpy(newmsg, msg, sizeof(dmmsg_t));
                Q_END(&(itab[ino].msgs), newmsg);
                msg->cmd = DM_DONTREPLY;
            } else {
                switch (((dmmsg_t*)Q_FIRST(itab[ino].msgs))->cmd) {
                  case DM_READC:
                    ((dmmsg_t*)Q_FIRST(itab[ino].msgs))->param.rwc.data = msg->param.rwc.data;
                    break;
                  case DM_WRITEC:
                    msg->param.rwc.data = ((dmmsg_t*)Q_FIRST(itab[ino].msgs))->param.rwc.data;
                    break;
                }
                /* release waiting task */
                send(((dmmsg_t*)Q_FIRST(itab[ino].msgs))->client, ((dmmsg_t*)Q_FIRST(itab[ino].msgs)));
                kfree(Q_REMV(&(itab[ino].msgs), Q_FIRST(itab[ino].msgs)));
            }
        }
        break;
    }
    return;
}
/*
 * =============
 */

#define DM_PIDOF(p) ((p) ? ((p)->pid) : (NULL))

static dm_task_t*
dm_findbypid (pid_t pid) {
    dm_task_t *it = (dm_task_t*)Q_FIRST(dm_task_q);
    while (it) {
        if (DM_PIDOF(it) == pid) {
            break;
        }
        it = (dm_task_t*)Q_NEXT(it);
    }
    return (it);
}

/*
 *
 */

static dm_task_t*
dm_removetask (dmmsg_t *msg) {
    int i;
    dm_task_t* pt = dm_findbypid(msg->param.adddel.pid);
    if (!pt) {
        return NULL;
    }
    for (i=0; i < MAX_FD; i++) {
        msg->param.openclose.fd = i;
        do_close(pt, msg);
    }
    kfree(Q_REMV(&dm_task_q, pt));
    return (pt);
}


/*
 * pid: task's own pid in kernel
 */

static void
dm_addnewtask (dmmsg_t *msg) {
    dm_task_t* pt;
    dm_task_t* parent;
    int i;
    pt = (dm_task_t*) kmalloc(sizeof(dm_task_t));
    if (!pt) {
        msg->param.adddel.pid = NULL;
        return;
    }
    memset(pt, 0x00, sizeof(dm_task_t));
    Q_END(&dm_task_q, pt);
    pt->pid = msg->param.adddel.pid;
    for (i=0; i < MAX_FD; i++) {
        pt->fd[i] = (-1);
    }
    if (!msg->param.adddel.parent) {
        return;  /* no parent, we're done */
    }
    parent = dm_findbypid(msg->param.adddel.parent);
    if (!parent) {
        /* Parent not found, ERROR */
        dm_removetask(msg);
        msg->param.adddel.pid = NULL;
        return;
    }
    for (i=0; i < MAX_FD; i++) {
        if (parent->fd[i] >= 0) {
            pt->fd[i] = parent->fd[i];
            filp[pt->fd[i]].refcnt++;
        }
    }
    return;
}

/*
 *
 */

void
dm (void) {
    pid_t client;
    dm_task_t *dm_client;
    dmmsg_t msg;


    dm_init();
    /* Let's go! */

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        dm_client = dm_findbypid(client);
        switch(msg.cmd){    

          case DM_ADDTASK:
            dm_addnewtask(&msg);
            break;

          case DM_DELTASK:
            dm_removetask(&msg);
            break;

          case DM_MKDEV:
            do_mkdev(&msg);
            break;

          case DM_MKNOD:
            do_mknod(&msg);
            break;

          case DM_DUP:
            do_dup(dm_client, &msg);
            break;

          case DM_PIPE:
            do_pipe(dm_client, &msg);
            break;

          case DM_STAT:
            do_stat(&msg);
            break;

          case DM_OPEN:
            do_open(dm_client, &msg);
            break;

          case DM_CLOSE:
            do_close(dm_client, &msg);
            break;

          case DM_INTERRUPT:
            sendrec(msg.client, &msg, sizeof(msg));
            client = msg.client;
            break;

          case DM_READC:
          case DM_WRITEC:
            msg.client = client;    /* May be delayed, save client */ 
            do_rw(dm_client, &msg);
            break;
        }
        if (msg.cmd != DM_DONTREPLY) {
            send(client, &msg);
        }
    }
}

/*
 *
 */

static pid_t            dmtask;

/*
 *
 */

pid_t
setdmpid (pid_t pid) {
    dmtask = pid;
    return (dmtask); 
}

/*
 *
 */

int
writec (int fd, int c) {
    dmmsg_t msg;
    msg.param.rwc.fd = fd;
    msg.param.rwc.data = c;
    msg.cmd = DM_WRITEC;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.rwc.data);
}

/*
 *
 */

int
readc (int fd) {
    dmmsg_t msg;
    msg.param.rwc.fd = fd;
    msg.cmd = DM_READC;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.rwc.data);
}

/*
 *
 */

pid_t
dm_addtask (pid_t pid, pid_t parent) {
    dmmsg_t msg;
    msg.cmd = DM_ADDTASK;
    msg.param.adddel.pid = pid;
    msg.param.adddel.parent = parent;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.adddel.pid);
}

/*
 *
 */

void
dm_deletetask (pid_t pid) {
    dmmsg_t msg;
    msg.cmd = DM_DELTASK;
    msg.param.adddel.pid = pid;
    sendrec(dmtask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

int
mkdev (void(*p)(void)) {
    dmmsg_t msg;
    msg.cmd = DM_MKDEV;
    msg.param.mkdev.ask.driver = p;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.mkdev.ans.id);
}

/*
 *
 */

int
mknod (int dev, char* name, mode_t mode) {
    dmmsg_t msg;
    msg.cmd = DM_MKNOD;
    msg.param.mknod.id = dev;
    msg.param.mknod.mode = mode;
    msg.param.mknod.name = name;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.mknod.ino);
}


int
pipe (int pipefd[2]) {
    dmmsg_t msg;
    msg.cmd = DM_PIPE;
    sendrec(dmtask, &msg, sizeof(msg));
    pipefd[0] = msg.param.pipe.fdi;
    pipefd[1] = msg.param.pipe.fdo;
    return (msg.param.pipe.result);
}
/* 
 *
 */

int
fstat (char *name, struct stat *st_stat) {
    dmmsg_t msg;
    msg.cmd = DM_STAT;
    msg.param.stat.ask.name = name;
    sendrec(dmtask, &msg, sizeof(msg));    
    memcpy(st_stat, &msg.param.stat.ans.st_stat, sizeof(struct stat));
    return (msg.param.stat.ans.code);
}

/* 
 *
 */

int
open (char *name) {
    dmmsg_t msg;
    msg.cmd = DM_OPEN;
    msg.param.openclose.name = name;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.openclose.fd);
}

/* 
 *
 */

void
close (int fd) {
    dmmsg_t msg;
    msg.cmd = DM_CLOSE;
    msg.param.openclose.fd = fd;
    sendrec(dmtask, &msg, sizeof(msg));
    return;
}

/* 
 *
 */

int
dup (int fd) {
    dmmsg_t msg;
    msg.cmd = DM_DUP;
    msg.param.dup.fd = fd;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.param.dup.fd);
}
