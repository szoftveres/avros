#include "../kernel/kernel.h"
#include "../lib/queue.h"
#include "../lib/mstddef.h"
#include "vfs.h"
#include "../drivers/drv.h"

/*
 * DEVTAB
 */

#define MAX_DEV         (6) /* u1, u2, null, pipe, eeprom, ramdisk */
static pid_t            devtab[MAX_DEV];
int                     pipedev_idx;
static int              debugn;

/*
 *  FILP
 */

typedef struct filp_s {
    int                 dev;
    int                 ino;
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
    int                 wdev;   /* working dir */
    int                 wino;
    int                 rdev;   /* root dir */
    int                 rino;
} vfs_task_t;

static q_head_t         vfs_task_q;


/*
 *
 */

static int
find_empty_devtab (void) {
    int i;
    for (i = 0; i < MAX_DEV; i++) {
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
find_empty_filp (void) {
    int i;
    for (i = 0; i < MAX_FILP; i++) {
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
    for (i = 0; i < MAX_FD; i++) {
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

static pid_t
setup_dev (vfsmsg_t *msg) {
    pid_t pid;
    pid = createtask(TASK_PRIO_HIGH, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE);
    setuptask(pid, msg->mkdev.ask.driver, msg->mkdev.ask.args, NULL);
    starttask(pid);
    return pid;
}

static void
do_mkdev (vfsmsg_t *msg) {
    int i;

    i = find_empty_devtab();
    if (i < 0) {
        msg->mkdev.ans.id = -1;
        return;
    }
    devtab[i] = setup_dev(msg);
    msg->mkdev.ans.id = i;
    return;
}

/*
 *   name = "abcd"    nlen == 4
 */
int
vfs_get_direntry (int *dev, int ino, char* name, int nlen) {
    vfsmsg_t msg;
    int i;
    char* n;

    msg.cmd = VFS_GET_DIRENTRY;
    msg.link.ino = ino;

    n = kmalloc(nlen + 1);
    if (!n) {
        return (-1);
    }
    for (i = 0; i != nlen; i++) {
        n[i] = name[i];
    }
    n[i] = '\0';
    msg.link.name = n;
    sendrec(devtab[*dev], &msg, sizeof(msg));
    kfree(n);
    return (msg.link.ino);
}

int
vfs_get_dir (vfs_task_t *client, int *dev, char* path) {
    int ino;

    if (!path || !path[0]) {
        return -1;
    } else if (path[0] == '/') {
        *dev = client->rdev;
        ino = client->rino;
        path += 1;
    } else {
        *dev = client->wdev;
        ino = client->wino;
    }
    while (ino >= 0) {
        int len;

        for (len = 0; (path[len] != '/') && (path[len] != '\0'); len++);
        if (path[len] == '\0') {
            break;
        }
        ino = vfs_get_direntry(dev, ino, path, len);
        path += (len + 1);
    }
    return (ino);
}

char*
basename (char* path, int *len) {
    char *bn;
    for (*len = 0, bn = path; *path; (*len) += 1, path++) {
        if (*path == '/') {
            *len = -1;
            bn = path + 1;
        }
    }
    return bn;
}

int
vfs_get_node (vfs_task_t *client, int *dev, char* path) {
    int ino;
    char *bn;
    int len;

    ino = vfs_get_dir(client, dev, path);
    if (ino < 0) {
        return -1;
    }
    bn = basename(path, &len);
    if (len > 0) {
        ino = vfs_get_direntry(dev, ino, bn, len);
    }
    return ino;
}


/*
 *
 */

static void
do_mknod (vfs_task_t *client, vfsmsg_t *msg) {
    int dev;
    int ino;
    int dir_ino;
    char *n;

    n = msg->mknod.name;

    /* Find the directory node */
    dir_ino = vfs_get_dir(client, &dev, msg->mknod.name);
    if (dir_ino < 0) {
        msg->mknod.ino = -1;
        return;
    }
    /* Create new node on device */
    sendrec(devtab[dev], msg, sizeof(vfsmsg_t));
    ino = msg->mknod.ino;

    /* Link the node */
    msg->cmd = VFS_LINK;
    msg->link.name = n;
    msg->link.ino = ino;
    msg->link.dir_ino = dir_ino;
    sendrec(devtab[dev], msg, sizeof(vfsmsg_t));
    if (msg->link.ino < 0) {
        msg->mknod.ino = -1;
        return; /* Cannot get node */
    }
    msg->mknod.ino = ino;
    return;
}

/*
 *
 */

static void
do_stat (vfs_task_t *client, vfsmsg_t *msg) {
    int dev;
    int ino;

    ino = vfs_get_node(client, &dev, msg->stat.ask.name);
    msg->stat.ans.code = (ino < 0 ? -1 : 0);
    msg->stat.ans.st_stat.dev = dev;
    msg->stat.ans.st_stat.ino = ino;
    return;
}

/*
 *
 */

static void
do_pipe (vfs_task_t *client, vfsmsg_t *msg) {
    int fd;
    int fp;
    int ino;

    /* Create inode on device */
    msg->cmd = VFS_MKNOD;
    sendrec(devtab[pipedev_idx], msg, sizeof(vfsmsg_t));
    if (msg->mknod.ino < 0) {
        msg->pipe.result = -1;
        return; /* Cannot create node */
    }
    ino = msg->mknod.ino;

    /* Get this new node  2x */
    msg->cmd = VFS_INODE_GRAB;
    msg->iget.ino = ino;
    sendrec(devtab[pipedev_idx], msg, sizeof(vfsmsg_t));

    if (msg->iget.ino < 0) {
        msg->pipe.result = -1;
        return; /* Cannot get node */
    }

    msg->cmd = VFS_INODE_GRAB;
    msg->iget.ino = ino;
    sendrec(devtab[pipedev_idx], msg, sizeof(vfsmsg_t));

    if (msg->iget.ino < 0) {
        msg->pipe.result = -1;
        return; /* Cannot get node */
    }

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
    filp[fp].dev = pipedev_idx;
    filp[fp].ino = ino;
    filp[fp].refcnt++;
    client->fd[fd] = fp;
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
    filp[fp].dev = pipedev_idx;
    filp[fp].ino = ino;
    filp[fp].refcnt++;
    client->fd[fd] = fp;
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

    fd = find_empty_fd(client);
    if (fd < 0) {
        msg->openclose.fd = -1;
        return;
    }
    fp = find_empty_filp();
    if (fp < 0) {
        msg->openclose.fd = -1;
        return;
    }

    filp[fp].dev = msg->openclose.dev;
    filp[fp].ino = msg->openclose.ino;
    filp[fp].pos = 0;

    /* Get the node */
    msg->cmd = VFS_INODE_GRAB;
    msg->iget.ino = filp[fp].ino;
    sendrec(devtab[filp[fp].dev], msg, sizeof(vfsmsg_t));

    if (msg->iget.ino < 0) {
        /* Node not found on dev */
        msg->openclose.fd = -1;
        return;
    }

    filp[fp].refcnt++;
    client->fd[fd] = fp;

    msg->openclose.fd = fd;
    return;
}

/*
 *
 */

static void
do_close (vfs_task_t *client, vfsmsg_t *msg) {
    int fp;

    if (msg->openclose.fd < 0) {
        return; /* Nothing to close */
    }
    fp = client->fd[msg->openclose.fd];
    client->fd[msg->openclose.fd] = (-1);
    if (fp < 0) {
        return; /* Nothing to close */
    }
    if (!(filp[fp].refcnt)) {
        return; /* Refcnt already zero, this is a serious error */
    }
    if ((--(filp[fp].refcnt))) {
        return; /* refcnt not zero yet */
    }

    /* No more refs, close this node */
    msg->cmd = VFS_INODE_RELEASE;
    msg->iget.ino = filp[fp].ino;
    sendrec(devtab[filp[fp].dev], msg, sizeof(vfsmsg_t));

    while (msg->cmd == VFS_REPEAT) {
        msg->cmd = VFS_FINAL;
        /* Unblocking waiting tasks(s) */
        send(msg->client, msg);
        sendrec(devtab[filp[fp].dev], msg, sizeof(vfsmsg_t));
    }
    if (msg->iget.ino < 0) {
        return; /* Node not found on dev */
    }

    return;
}

/*
 *
 */

static void
do_rw (vfs_task_t *client, vfsmsg_t *msg) {
    int fp;

    if (msg->rw.fd < 0) {
        msg->rw.data = EOF; /* wrong fd */
        return;
    }
    fp = client->fd[msg->rw.fd];

    if (fp < 0) {
        msg->rw.data = EOF; /* wrong fd */
        return;
    }

    msg->rw.ino = filp[fp].ino;
    msg->rw.pos = filp[fp].pos;

    sendrec(devtab[filp[fp].dev], msg, sizeof(vfsmsg_t));

    while (msg->cmd == VFS_REPEAT) {
        msg->cmd = VFS_FINAL;
        if (msg->client) {
            /* Unblocking waiting tasks(s) */
            send(msg->client, msg);
        }
        sendrec(devtab[filp[fp].dev], msg, sizeof(vfsmsg_t));
    }
    filp[fp].pos += msg->rw.bnum;

    return;
}


static void
do_int (vfsmsg_t *msg) {
    pid_t client = msg->client;
    sendrec(client, msg, sizeof(vfsmsg_t));
    while (msg->cmd == VFS_REPEAT) {
        msg->cmd = VFS_FINAL;
        if (msg->client) {
            /* Unblocking waiting tasks(s) */
            send(msg->client, msg);
        }
        sendrec(client, msg, sizeof(vfsmsg_t));
    }
    return;
}
/*
 * =============
 */

static vfs_task_t*
vfs_findbypid (pid_t pid) {
    vfs_task_t *it = (vfs_task_t*)Q_FIRST(vfs_task_q);
    while (it) {
        if (it->pid == pid) {
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
    for (i = 0; i < MAX_FD; i++) {
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
    for (i = 0; i != MAX_FD; i++) {
        pt->fd[i] = (-1);
    }
    if (!msg->adddel.parent) {
        pt->rdev = 1;   /* XXX hack until better idea comes */
        pt->rino = 0;
        pt->wdev = 1;
        pt->wino = 0;
        return;  /* no parent, we're done */
    }
    parent = vfs_findbypid(msg->adddel.parent);
    if (!parent) {
        /* Parent not found, ERROR */
        vfs_removetask(msg);
        msg->adddel.pid = NULL;
        return;
    }
    pt->rdev = parent->rdev;
    pt->rino = parent->rino;
    pt->wdev = parent->wdev;
    pt->wino = parent->wino;
    for (i = 0; i != MAX_FD; i++) {
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

static void
vfs_init (vfsmsg_t *msg) {
    memset(filp, 0, (sizeof(filp)));
    memset(devtab, 0, (sizeof(devtab)));
    q_init(&vfs_task_q);

    /* set up pipe device */
    msg->cmd = VFS_MKDEV;
    msg->mkdev.ask.driver = pipedev;
    msg->mkdev.ask.args = NULL;
    do_mkdev(msg);
    pipedev_idx = msg->mkdev.ans.id;

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

    kirqdis();
    vfs_init(&msg);

    debugn = 0;
    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        vfs_client = vfs_findbypid(client);
        switch (msg.cmd) {

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
            do_mknod(vfs_client, &msg);
            break;

          case VFS_DUP:
            do_dup(vfs_client, &msg);
            break;

          case VFS_PIPE:
            do_pipe(vfs_client, &msg);
            break;

          case VFS_STAT:
            do_stat(vfs_client, &msg);
            break;

          case VFS_OPEN:
            do_open(vfs_client, &msg);
            break;

          case VFS_CLOSE:
            do_close(vfs_client, &msg);
            break;

          case VFS_RD_INTERRUPT:
          case VFS_WR_INTERRUPT:
            do_int(&msg);
            client = msg.client;
            break;

          case VFS_READC:
          case VFS_WRITEC:
            msg.client = client;    /* May be delayed, save client */
            do_rw(vfs_client, &msg);
            break;

          case VFS_DEBUG:
            if (msg.interrupt.data) {
                debugn = 0;
            }
            msg.interrupt.data = debugn;
            break;
        }

        if ((msg.cmd != VFS_HOLD) && client) {
            msg.cmd = VFS_FINAL;
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

void
vfs_rd_interrupt (pid_t driver) {
    vfsmsg_t msg;
    msg.client = driver;
    msg.cmd = VFS_RD_INTERRUPT;
    send(vfstask, &msg);
}

void
vfs_wr_interrupt (pid_t driver) {
    vfsmsg_t msg;
    msg.client = driver;
    msg.cmd = VFS_WR_INTERRUPT;
    send(vfstask, &msg);
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
vfs_createtask (pid_t pid, pid_t parent) {
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
mknod (int dev, char* name) {
    vfsmsg_t msg;
    msg.cmd = VFS_MKNOD;
    msg.mknod.dev = dev;
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

int
vfs_debugn (int reset) {
    vfsmsg_t msg;
    msg.cmd = VFS_DEBUG;
    msg.interrupt.data = reset;
    sendrec(vfstask, &msg, sizeof(msg));
    return (msg.interrupt.data);
}


