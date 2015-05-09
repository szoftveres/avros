#include "kernel.h"
#include "dm.h"
#include "queue.h"

/*
 * DEVTAB
 */

#define MAX_DEV         (6) /* u1, u2, null, pipe, eeprom, ramdisk */
static pid_t            devtab[MAX_DEV];


/*
 * VNODE
 */

typedef struct vnode_s {
    int                 dev;
    int                 ino;    /* inode number on device */
    char                refcnt;
    mode_t              mode;
    union {
        q_head_t            msgs;
    };
} vnode_t;



#define MAX_I           (12)
static vnode_t          itab[MAX_I];

/*
 *  FILP
 */

typedef struct filp_s {
    int                 vidx;
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
        if (!itab[i].refcnt) {
            return i; 
        }
    }
    return (-1);
}


static int
find_open_itab (int dev, int ino) {
    int i;
    for (i=0; i < MAX_I; i++) {
        if ((itab[i].refcnt) && 
            (itab[i].dev == dev) &&
            (itab[i].ino == ino)) {
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
 * ====================================================
 */


static void
do_dup (dm_task_t *client, dmmsg_t *msg) {
    int fp;
    int fd;
    if (msg->dup.fd < 0) {
        msg->dup.fd = -1;  /* Nothing to dup */
        return;
    }
    fp = client->fd[msg->dup.fd];
    if (fp < 0) {
        msg->dup.fd = -1;  /* Nothing to dup */
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->dup.fd = -1;  /* No free fd */
        return;
    }
    client->fd[fd] = fp;
    filp[fp].refcnt++;
    msg->dup.fd = fd;
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
        msg->mkdev.ans.id = -1;
        return;
    }
    pid = addtask(TASK_PRIO_HIGH);
    launchtask(pid, msg->mkdev.ask.driver, DEFAULT_STACK_SIZE);
    devtab[i] = pid;

    msg->cmd = DM_MKDEV;
    sendrec(pid, msg, sizeof(dmmsg_t));
    msg->mkdev.ans.id = i;
    return;
}

/*
 *
 */

static void
do_mknod (dmmsg_t *msg) {
    int dev;
    int ino;

    dev = msg->mknod.id; 
    /* Create node on device */
    sendrec(devtab[dev], msg, sizeof(dmmsg_t));
    ino = msg->mknod.ino;

    /* Link the node */
    msg->cmd = DM_LINK;
    msg->iget.ask.ino = ino;
    sendrec(devtab[dev], msg, sizeof(dmmsg_t));

    msg->mknod.ino = ino;
    return;
}


/* 
 *
 */

static void
do_stat (dmmsg_t *msg) {

    msg->stat.ans.st_stat.ino = msg->stat.ans.st_stat.ino;
    /*
    msg->stat.ans.st_stat.dev = itab[dirent->ino].dev;
    msg->stat.ans.st_stat.size = itab[dirent->ino].size;
    msg->stat.ans.st_stat.mode = itab[dirent->ino].mode;
    msg->stat.ans.code = 0;
    */
    return;
}

/*
 *
 */

static void
do_pipe (dm_task_t *client, dmmsg_t *msg) {
    int fd;
    int fp;
    int vidx;
    
    vidx = find_empty_itab();            
    if (vidx < 0) {
        msg->pipe.result = -1;
        return;
    }

    /* Assumption: device 0 is PIPE */
    itab[vidx].dev = 0;

    /* Create inode on device */
    msg->cmd = DM_MKNOD;
    msg->mknod.mode = S_IFIFO;
    sendrec(devtab[itab[vidx].dev], msg, sizeof(dmmsg_t));
    itab[vidx].ino = msg->mknod.ino;

    /* Get this new node */
    msg->cmd = DM_IGET; 
    msg->iget.ask.ino = itab[vidx].ino;
    sendrec(devtab[itab[vidx].dev], msg, sizeof(dmmsg_t));
    itab[vidx].mode = msg->iget.ans.mode;
    
    /* input */
    fp = find_empty_filp();
    if (fp < 0) {
        msg->pipe.result = -1;
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->pipe.result = -1;
        return;
    }
    client->fd[fd] = fp;
    filp[fp].vidx = vidx;
    filp[fp].refcnt++;
    itab[vidx].refcnt++; 
    msg->pipe.fdi = fd;

    /* output */
    fp = find_empty_filp();
    if (fp < 0) {
        msg->pipe.result = -1;
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->pipe.result = -1;
        return;
    }
    client->fd[fd] = fp;
    filp[fp].vidx = vidx;
    filp[fp].refcnt++;
    itab[vidx].refcnt++;
 
    q_init(&(itab[vidx].msgs));
    msg->pipe.fdo = fd;

    msg->pipe.result = 0;
    return;
}

/*
 *
 */

static void
do_open (dm_task_t *client, dmmsg_t *msg) {
    int fd;
    int fp;
    int vidx;

    vidx = find_open_itab(msg->openclose.dev, msg->openclose.ino);
    /* Check if this node is already open */
    if (vidx < 0) {
        vidx = find_empty_itab();    
        if (vidx < 0) {
            msg->openclose.fd = -1;
            return;
        }
        itab[vidx].dev = msg->openclose.dev;
        itab[vidx].ino = msg->openclose.ino;

        /* Get the node */
        msg->cmd = DM_IGET;
        msg->iget.ask.ino = itab[vidx].ino;
        sendrec(devtab[itab[vidx].dev], msg, sizeof(dmmsg_t));
        itab[vidx].mode = msg->iget.ans.mode;
    }

    fp = find_empty_filp();
    if (fp < 0) {
        msg->openclose.fd = -1;
        return;
    }
    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->openclose.fd = -1;
        return;
    }
    client->fd[fd] = fp;
    filp[fp].pos = 0;
    filp[fp].vidx = vidx;
    filp[fp].refcnt++;
    itab[vidx].refcnt++;
    q_init(&(itab[vidx].msgs));
    msg->openclose.fd = fd;
    return;
}

/*
 *
 */

static void
do_close (dm_task_t *client, dmmsg_t *msg) {
    int fp;
    int vidx;
    int fd = msg->openclose.fd;
    dmmsg_t* msg_p; /* Temporary pointer */
    

    if (fd < 0) {
        return; /* Nothing to close */
    }   
    fp = client->fd[fd];
    if (fp < 0) {
        return; /* Nothing to close */
    }
    vidx = filp[fp].vidx;
    client->fd[fd] = (-1);

    if ((--(filp[fp].refcnt))) {
        return; /* refcnt not zero yet */
    }
    if (itab[vidx].mode == S_IFIFO) {
        /* One end of the pipe has closed,
         * hence sending EOF to waiting clients */
        while(!Q_EMPTY(itab[vidx].msgs)) {
            msg_p = (dmmsg_t*) Q_FIRST(itab[vidx].msgs);
            msg_p->rw.data = EOF;
            send(msg_p->client, msg_p);
            kfree(Q_REMV(&(itab[vidx].msgs), msg_p));
        }
    }

    if (--(itab[vidx].refcnt)) {
        return; /* refcnt not zero yet */
    }

    /* No more refs, close this node */
    msg->cmd = DM_IPUT;
    msg->iput.ask.ino = itab[vidx].ino;
    sendrec(devtab[itab[vidx].dev], msg, sizeof(dmmsg_t));

    return;
}

/*
 *
 */

static void
do_rw (dm_task_t *client, dmmsg_t *msg) {
    int fd = msg->rw.fd;
    int vidx;
    dmmsg_t* msg_p; /* Temporary pointer */

    if ((fd < 0) || (client->fd[fd] < 0)) {
        msg->rw.data = EOF; /* wrong fd */
        return;
    }
    vidx = filp[client->fd[fd]].vidx;
    msg->rw.ino = itab[vidx].ino;
    /* no need for FD from this point */

    switch (itab[vidx].mode) {
      case S_IFREG:
        msg->rw.pos = filp[client->fd[fd]].pos;
        sendrec(devtab[itab[vidx].dev], msg, sizeof(dmmsg_t));  
        filp[client->fd[fd]].pos += msg->rw.bnum;
        break;

      case S_IFCHR:
        sendrec(devtab[itab[vidx].dev], msg, sizeof(dmmsg_t));
        break;

      case S_IFIFO:
        if (Q_EMPTY(itab[vidx].msgs)) {   /* Empty pipe */
            if (itab[vidx].refcnt <= 1) {
                /* Other end detached, send EOF */
                msg->rw.data = EOF;
            } else {
                /* FIFO empty, save request */
                msg_p = (dmmsg_t*) kmalloc(sizeof(dmmsg_t));
                memcpy(msg_p, msg, sizeof(dmmsg_t));
                Q_END(&(itab[vidx].msgs), msg_p);
                msg->cmd = DM_DONTREPLY;
            }
        } else {        /* Read or Write requests in the pipe */
            msg_p = (dmmsg_t*) Q_FIRST(itab[vidx].msgs);
            if (msg_p->cmd == msg->cmd) {
                /* Same request type, save it */
                msg_p = (dmmsg_t*) kmalloc(sizeof(dmmsg_t));
                memcpy(msg_p, msg, sizeof(dmmsg_t));
                Q_END(&(itab[vidx].msgs), msg_p);
                msg->cmd = DM_DONTREPLY;
            } else {
                switch (msg_p->cmd) {
                  case DM_READC:
                    msg_p->rw.data = msg->rw.data;
                    break;
                  case DM_WRITEC:
                    msg->rw.data = msg_p->rw.data;
                    break;
                }
                /* release waiting task */
                send(msg_p->client, msg_p);
                kfree(Q_REMV(&(itab[vidx].msgs), msg_p));
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
    dm_task_t* pt = dm_findbypid(msg->adddel.pid);
    if (!pt) {
        return NULL;
    }
    for (i=0; i < MAX_FD; i++) {
        msg->openclose.fd = i;
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
        msg->adddel.pid = NULL;
        return;
    }
    memset(pt, 0x00, sizeof(dm_task_t));
    Q_END(&dm_task_q, pt);
    pt->pid = msg->adddel.pid;
    for (i=0; i < MAX_FD; i++) {
        pt->fd[i] = (-1);
    }
    if (!msg->adddel.parent) {
        return;  /* no parent, we're done */
    }
    parent = dm_findbypid(msg->adddel.parent);
    if (!parent) {
        /* Parent not found, ERROR */
        dm_removetask(msg);
        msg->adddel.pid = NULL;
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
    msg.rw.fd = fd;
    msg.rw.data = c;
    msg.cmd = DM_WRITEC;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.rw.data);
}

/*
 *
 */

int
readc (int fd) {
    dmmsg_t msg;
    msg.rw.fd = fd;
    msg.cmd = DM_READC;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.rw.data);
}

/*
 *
 */

pid_t
dm_addtask (pid_t pid, pid_t parent) {
    dmmsg_t msg;
    msg.cmd = DM_ADDTASK;
    msg.adddel.pid = pid;
    msg.adddel.parent = parent;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.adddel.pid);
}

/*
 *
 */

void
dm_deletetask (pid_t pid) {
    dmmsg_t msg;
    msg.cmd = DM_DELTASK;
    msg.adddel.pid = pid;
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
    msg.mkdev.ask.driver = p;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.mkdev.ans.id);
}

/*
 *
 */

int
mknod (int dev, char* name, mode_t mode) {
    dmmsg_t msg;
    msg.cmd = DM_MKNOD;
    msg.mknod.id = dev;
    msg.mknod.mode = mode;
    msg.mknod.name = name;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.mknod.ino);
}


int
pipe (int pipefd[2]) {
    dmmsg_t msg;
    msg.cmd = DM_PIPE;
    sendrec(dmtask, &msg, sizeof(msg));
    pipefd[0] = msg.pipe.fdi;
    pipefd[1] = msg.pipe.fdo;
    return (msg.pipe.result);
}
/* 
 *
 */

int
fstat (char *name, struct stat *st_stat) {
    dmmsg_t msg;
    msg.cmd = DM_STAT;
    msg.stat.ask.name = name;
    sendrec(dmtask, &msg, sizeof(msg));    
    memcpy(st_stat, &msg.stat.ans.st_stat, sizeof(struct stat));
    return (msg.stat.ans.code);
}

/* 
 *
 */

int
open (char *name) {
    dmmsg_t msg;
    int i;

    for(i=0; name[i]; i++) {
        if (name[i] == '/') {
            name[i] = '\0';
            i++;
            break;
        }
    }
    msg.cmd = DM_OPEN;
    msg.openclose.dev = atoi(name);
    msg.openclose.ino = atoi(&(name[i]));
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.openclose.fd);
}

/* 
 *
 */

void
close (int fd) {
    dmmsg_t msg;
    msg.cmd = DM_CLOSE;
    msg.openclose.fd = fd;
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
    msg.dup.fd = fd;
    sendrec(dmtask, &msg, sizeof(msg));
    return (msg.dup.fd);
}
