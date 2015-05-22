#include "kernel.h"
#include "vfs.h"
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



#define MAX_VNODE           (12)
static vnode_t          vtab[MAX_VNODE];

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

typedef struct vfs_task_s {
    QUEUE_HEADER
    pid_t               pid;
    int                 fd[MAX_FD];
} vfs_task_t;

static q_head_t         vfs_task_q;

/*
 *
 */

static void
vfs_init (void) {
    memset(filp, 0, (sizeof(filp)));
    memset(devtab, 0, (sizeof(devtab)));
    memset(vtab, 0, (sizeof(vtab)));
    q_init(&vfs_task_q); 
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
find_empty_vtab (void) {
    int i;
    for (i=0; i < MAX_VNODE; i++) {
        if (!vtab[i].refcnt) {
            return i; 
        }
    }
    return (-1);
}


static int
find_open_vtab (int dev, int ino) {
    int i;
    for (i=0; i < MAX_VNODE; i++) {
        if ((vtab[i].refcnt) && 
            (vtab[i].dev == dev) &&
            (vtab[i].ino == ino)) {
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
find_empty_fd (vfs_task_t *client) {
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
do_dup (vfs_task_t *client, vfsmsg_t *msg) {
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
do_mkdev (vfsmsg_t *msg) {
    int i;
    pid_t pid;

    i = find_empty_devtab();
    if (i < 0) {
        msg->mkdev.ans.id = -1;
        return;
    }
    pid = cratetask(TASK_PRIO_HIGH);
    launchtask(pid, msg->mkdev.ask.driver, msg->mkdev.ask.args, NULL, DEFAULT_STACK_SIZE);
    devtab[i] = pid;

    msg->cmd = VFS_MKDEV;
    sendrec(pid, msg, sizeof(vfsmsg_t));
    msg->mkdev.ans.id = i;
    return;
}

/*
 *
 */

static void
do_mknod (vfsmsg_t *msg) {
    int dev;
    int ino;

    dev = msg->mknod.id; 
    /* Create node on device */
    sendrec(devtab[dev], msg, sizeof(vfsmsg_t));
    ino = msg->mknod.ino;

    /* Link the node */
    msg->cmd = VFS_LINK;
    msg->iget.ask.ino = ino;
    sendrec(devtab[dev], msg, sizeof(vfsmsg_t));

    msg->mknod.ino = ino;
    return;
}


/* 
 *
 */

static void
do_stat (vfsmsg_t *msg) {

    msg->stat.ans.st_stat.ino = msg->stat.ans.st_stat.ino;
    /*
    msg->stat.ans.st_stat.dev = vtab[dirent->ino].dev;
    msg->stat.ans.st_stat.size = vtab[dirent->ino].size;
    msg->stat.ans.st_stat.mode = vtab[dirent->ino].mode;
    msg->stat.ans.code = 0;
    */
    return;
}

/*
 *
 */

static void
do_pipe (vfs_task_t *client, vfsmsg_t *msg) {
    int fd;
    int fp;
    int vidx;
    
    vidx = find_empty_vtab();            
    if (vidx < 0) {
        msg->pipe.result = -1;
        return;
    }

    /* Assumption: device 0 is PIPE */
    vtab[vidx].dev = 0;

    /* Create inode on device */
    msg->cmd = VFS_MKNOD;
    msg->mknod.mode = S_IFIFO;
    sendrec(devtab[vtab[vidx].dev], msg, sizeof(vfsmsg_t));
    vtab[vidx].ino = msg->mknod.ino;

    /* Get this new node */
    msg->cmd = VFS_IGET; 
    msg->iget.ask.ino = vtab[vidx].ino;
    sendrec(devtab[vtab[vidx].dev], msg, sizeof(vfsmsg_t));
    vtab[vidx].mode = msg->iget.ans.mode;
    
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
    vtab[vidx].refcnt++; 
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
    vtab[vidx].refcnt++;
 
    q_init(&(vtab[vidx].msgs));
    msg->pipe.fdo = fd;

    msg->pipe.result = 0;
    return;
}

/*
 *
 */

static void
do_open (vfs_task_t *client, vfsmsg_t *msg) {
    int fd;
    int fp;
    int vidx;

    vidx = find_open_vtab(msg->openclose.dev, msg->openclose.ino);
    /* Check if this node is already open */
    if (vidx < 0) {
        vidx = find_empty_vtab();    
        if (vidx < 0) {
            msg->openclose.fd = -1;
            return;
        }
        vtab[vidx].dev = msg->openclose.dev;
        vtab[vidx].ino = msg->openclose.ino;

        /* Get the node */
        msg->cmd = VFS_IGET;
        msg->iget.ask.ino = vtab[vidx].ino;
        sendrec(devtab[vtab[vidx].dev], msg, sizeof(vfsmsg_t));
        vtab[vidx].mode = msg->iget.ans.mode;
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
    vtab[vidx].refcnt++;
    q_init(&(vtab[vidx].msgs));
    msg->openclose.fd = fd;
    return;
}

/*
 *
 */

static void
do_close (vfs_task_t *client, vfsmsg_t *msg) {
    int fp;
    int vidx;
    int fd = msg->openclose.fd;
    vfsmsg_t* msg_p; /* Temporary pointer */
    

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
    if (vtab[vidx].mode == S_IFIFO) {
        /* One end of the pipe has closed,
         * hence sending EOF to waiting clients */
        while(!Q_EMPTY(vtab[vidx].msgs)) {
            msg_p = (vfsmsg_t*) Q_FIRST(vtab[vidx].msgs);
            msg_p->rw.data = EOF;
            send(msg_p->client, msg_p);
            kfree(Q_REMV(&(vtab[vidx].msgs), msg_p));
        }
    }

    if (--(vtab[vidx].refcnt)) {
        return; /* refcnt not zero yet */
    }

    /* No more refs, close this node */
    msg->cmd = VFS_IPUT;
    msg->iput.ask.ino = vtab[vidx].ino;
    sendrec(devtab[vtab[vidx].dev], msg, sizeof(vfsmsg_t));

    return;
}

/*
 *
 */

static void
do_rw (vfs_task_t *client, vfsmsg_t *msg) {
    int fd = msg->rw.fd;
    int vidx;
    vfsmsg_t* msg_p; /* Temporary pointer */

    if ((fd < 0) || (client->fd[fd] < 0)) {
        msg->rw.data = EOF; /* wrong fd */
        return;
    }
    vidx = filp[client->fd[fd]].vidx;
    msg->rw.ino = vtab[vidx].ino;
    /* no need for FD from this point */

    switch (vtab[vidx].mode) {
      case S_IFREG:
        msg->rw.pos = filp[client->fd[fd]].pos;
        sendrec(devtab[vtab[vidx].dev], msg, sizeof(vfsmsg_t));  
        filp[client->fd[fd]].pos += msg->rw.bnum;
        break;

      case S_IFCHR:
        sendrec(devtab[vtab[vidx].dev], msg, sizeof(vfsmsg_t));
        break;

      case S_IFIFO:
        if (Q_EMPTY(vtab[vidx].msgs)) {   /* Empty pipe */
            if (vtab[vidx].refcnt <= 1) {
                /* Other end detached, send EOF */
                msg->rw.data = EOF;
            } else {
                /* FIFO empty, save request */
                msg_p = (vfsmsg_t*) kmalloc(sizeof(vfsmsg_t));
                memcpy(msg_p, msg, sizeof(vfsmsg_t));
                Q_END(&(vtab[vidx].msgs), msg_p);
                msg->cmd = VFS_DONTREPLY;
            }
        } else {        /* Read or Write requests in the pipe */
            msg_p = (vfsmsg_t*) Q_FIRST(vtab[vidx].msgs);
            if (msg_p->cmd == msg->cmd) {
                /* Same request type, save it */
                msg_p = (vfsmsg_t*) kmalloc(sizeof(vfsmsg_t));
                memcpy(msg_p, msg, sizeof(vfsmsg_t));
                Q_END(&(vtab[vidx].msgs), msg_p);
                msg->cmd = VFS_DONTREPLY;
            } else {
                switch (msg_p->cmd) {
                  case VFS_READC:
                    msg_p->rw.data = msg->rw.data;
                    break;
                  case VFS_WRITEC:
                    msg->rw.data = msg_p->rw.data;
                    break;
                }
                /* release waiting task */
                send(msg_p->client, msg_p);
                kfree(Q_REMV(&(vtab[vidx].msgs), msg_p));
            }
        }
        break;
    }
    return;
}
/*
 * =============
 */

#define VFS_PIDOF(p) ((p) ? ((p)->pid) : (NULL))

static vfs_task_t*
vfs_findbypid (pid_t pid) {
    vfs_task_t *it = (vfs_task_t*)Q_FIRST(vfs_task_q);
    while (it) {
        if (VFS_PIDOF(it) == pid) {
            break;
        }
        it = (vfs_task_t*)Q_NEXT(it);
    }
    return (it);
}

/*
 *
 */

static vfs_task_t*
vfs_removetask (vfsmsg_t *msg) {
    int i;
    vfs_task_t* pt = vfs_findbypid(msg->adddel.pid);
    if (!pt) {
        return NULL;
    }
    for (i=0; i < MAX_FD; i++) {
        msg->openclose.fd = i;
        do_close(pt, msg);
    }
    kfree(Q_REMV(&vfs_task_q, pt));
    return (pt);
}


/*
 * pid: task's own pid in kernel
 */

static void
vfs_addnewtask (vfsmsg_t *msg) {
    vfs_task_t* pt;
    vfs_task_t* parent;
    int i;
    pt = (vfs_task_t*) kmalloc(sizeof(vfs_task_t));
    if (!pt) {
        msg->adddel.pid = NULL;
        return;
    }
    memset(pt, 0x00, sizeof(vfs_task_t));
    Q_END(&vfs_task_q, pt);
    pt->pid = msg->adddel.pid;
    for (i=0; i < MAX_FD; i++) {
        pt->fd[i] = (-1);
    }
    if (!msg->adddel.parent) {
        return;  /* no parent, we're done */
    }
    parent = vfs_findbypid(msg->adddel.parent);
    if (!parent) {
        /* Parent not found, ERROR */
        vfs_removetask(msg);
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
vfs (void* args UNUSED) {
    pid_t client;
    vfs_task_t *vfs_client;
    vfsmsg_t msg;


    vfs_init();
    /* Let's go! */

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        vfs_client = vfs_findbypid(client);
        switch(msg.cmd){    

          case VFS_ADDTASK:
            vfs_addnewtask(&msg);
            break;

          case VFS_DELTASK:
            vfs_removetask(&msg);
            break;

          case VFS_MKDEV:
            do_mkdev(&msg);
            break;

          case VFS_MKNOD:
            do_mknod(&msg);
            break;

          case VFS_DUP:
            do_dup(vfs_client, &msg);
            break;

          case VFS_PIPE:
            do_pipe(vfs_client, &msg);
            break;

          case VFS_STAT:
            do_stat(&msg);
            break;

          case VFS_OPEN:
            do_open(vfs_client, &msg);
            break;

          case VFS_CLOSE:
            do_close(vfs_client, &msg);
            break;

          case VFS_INTERRUPT:
            sendrec(msg.client, &msg, sizeof(msg));
            client = msg.client;
            break;

          case VFS_READC:
          case VFS_WRITEC:
            msg.client = client;    /* May be delayed, save client */ 
            do_rw(vfs_client, &msg);
            break;
        }
        if (msg.cmd != VFS_DONTREPLY) {
            send(client, &msg);
        }
    }
}

/*
 *
 */

static pid_t            vfstask;

/*
 *
 */

pid_t
setvfspid (pid_t pid) {
    vfstask = pid;
    return (vfstask); 
}

/*
 *
 */

int
writec (int fd, int c) {
    vfsmsg_t msg;
    msg.rw.fd = fd;
    msg.rw.data = c;
    msg.cmd = VFS_WRITEC;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.rw.data);
}

/*
 *
 */

int
readc (int fd) {
    vfsmsg_t msg;
    msg.rw.fd = fd;
    msg.cmd = VFS_READC;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.rw.data);
}

/*
 *
 */

pid_t
vfs_cratetask (pid_t pid, pid_t parent) {
    vfsmsg_t msg;
    msg.cmd = VFS_ADDTASK;
    msg.adddel.pid = pid;
    msg.adddel.parent = parent;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.adddel.pid);
}

/*
 *
 */

void
vfs_deletetask (pid_t pid) {
    vfsmsg_t msg;
    msg.cmd = VFS_DELTASK;
    msg.adddel.pid = pid;
    sendrec(vfstask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

int
mkdev (void(*p)(void* args), void* args) {
    vfsmsg_t msg;
    msg.cmd = VFS_MKDEV;
    msg.mkdev.ask.driver = p;
    msg.mkdev.ask.args = args;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.mkdev.ans.id);
}

/*
 *
 */

int
mknod (int dev, char* name, mode_t mode) {
    vfsmsg_t msg;
    msg.cmd = VFS_MKNOD;
    msg.mknod.id = dev;
    msg.mknod.mode = mode;
    msg.mknod.name = name;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.mknod.ino);
}


int
pipe (int pipefd[2]) {
    vfsmsg_t msg;
    msg.cmd = VFS_PIPE;
    sendrec(vfstask, &msg, sizeof(msg));
    pipefd[0] = msg.pipe.fdi;
    pipefd[1] = msg.pipe.fdo;
    return (msg.pipe.result);
}
/* 
 *
 */

int
fstat (char *name, struct stat *st_stat) {
    vfsmsg_t msg;
    msg.cmd = VFS_STAT;
    msg.stat.ask.name = name;
    sendrec(vfstask, &msg, sizeof(msg));    
    memcpy(st_stat, &msg.stat.ans.st_stat, sizeof(struct stat));
    return (msg.stat.ans.code);
}

/* 
 *
 */

int
open (char *name) {
    vfsmsg_t msg;
    int i;

    for(i=0; name[i]; i++) {
        if (name[i] == '/') {
            name[i] = '\0';
            i++;
            break;
        }
    }
    msg.cmd = VFS_OPEN;
    msg.openclose.dev = atoi(name);
    msg.openclose.ino = atoi(&(name[i]));
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.openclose.fd);
}

/* 
 *
 */

void
close (int fd) {
    vfsmsg_t msg;
    msg.cmd = VFS_CLOSE;
    msg.openclose.fd = fd;
    sendrec(vfstask, &msg, sizeof(msg));
    return;
}

/* 
 *
 */

int
dup (int fd) {
    vfsmsg_t msg;
    msg.cmd = VFS_DUP;
    msg.dup.fd = fd;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.dup.fd);
}
