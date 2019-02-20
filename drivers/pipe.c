#include "../kernel/kernel.h"

#include "../servers/vfs.h"
#include "../lib/queue.h"
#include "../lib/mstddef.h"

#include "drv.h"


#define PD_MAX_NODES 8



typedef struct pdnode_s {
    char        refcnt;
    char        links;
    q_head_t    msgs;
} pdnode_t;

int
pd_find_empty_node (pdnode_t** list) {
    int i;
    for (i = 0; i != PD_MAX_NODES; i++) {
        if (!list[i]) {
            return i;
        }
    }
    return (-1);
}

void pipedev (void* args UNUSED) {
    pid_t               client;
    vfsmsg_t            msg;
    vfsmsg_container_t* container; /* Temporary pointer */

    pdnode_t** nodes = (pdnode_t**)kmalloc(sizeof(pdnode_t*) * PD_MAX_NODES);
    memset(nodes, 0, (sizeof(pdnode_t*) * PD_MAX_NODES));
    kirqdis();

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch (msg.cmd) {
          case VFS_MKNOD:
            msg.mknod.ino = pd_find_empty_node(nodes);
            nodes[msg.mknod.ino] = (pdnode_t*)kmalloc(sizeof(pdnode_t));
            nodes[msg.mknod.ino]->refcnt = 0;
            nodes[msg.mknod.ino]->links = 0;
            q_init(&(nodes[msg.mknod.ino]->msgs));
            break;


          case VFS_LINK:
            if (!nodes[msg.link.ino]) {
                msg.link.ino = -1;
                break;
            }
            nodes[msg.link.ino]->links += 1;
            break;
          case VFS_UNLINK:
            if (!nodes[msg.link.ino]) {
                msg.link.ino = -1;
                break;
            }
            nodes[msg.link.ino]->links -= 1;
            if (nodes[msg.link.ino]->refcnt ||
                nodes[msg.link.ino]->links) {
                /* more refs */
                break;
            }
            kfree(nodes[msg.link.ino]);
            nodes[msg.link.ino] = NULL;
            break;

          case VFS_INODE_GRAB:
            if (!nodes[msg.iget.ino]) {
                msg.iget.ino = -1;
                break;
            }
            nodes[msg.iget.ino]->refcnt += 1;
            break;
          case VFS_INODE_RELEASE:
            if (!nodes[msg.iget.ino]) {
                msg.iget.ino = -1;
                break;
            }
            nodes[msg.iget.ino]->refcnt -= 1;

            /*
             * One end detached and no links, notify
             * waiting tasks and empty pipe
             */
            if (nodes[msg.iget.ino]->refcnt == 1) {
                while (!Q_EMPTY(nodes[msg.iget.ino]->msgs)) {
                    container = (vfsmsg_container_t*) Q_FIRST(nodes[msg.iget.ino]->msgs);
                    container->msg.rw.data = EOF;
                    container->msg.rw.bnum = 0;
                    container->msg.cmd = VFS_REPEAT;
                    sendrec(client, &(container->msg), sizeof(vfsmsg_t));
                    kfree(Q_REMV(&(nodes[msg.iget.ino]->msgs), container));
                }
                break;
            }

            if (nodes[msg.iget.ino]->refcnt) {
                /* more refs */
                break;
            }

            if (nodes[msg.iget.ino]->links) {
                /* More links, don't destroy */
                break;
            }

            kfree(nodes[msg.iget.ino]);
            nodes[msg.iget.ino] = NULL;
            break;
          case VFS_WRITEC:
          case VFS_READC:
            if (Q_EMPTY(nodes[msg.rw.ino]->msgs)) {   /* Empty pipe */
                if ((nodes[msg.rw.ino]->refcnt <= 1) &&
                    (!nodes[msg.rw.ino]->links)) {
                    /* Other end detached, send EOF */
                    msg.rw.data = EOF;
                } else {
                    /* FIFO empty, save request */
                    container = (vfsmsg_container_t*) kmalloc(sizeof(vfsmsg_container_t));
                    memcpy(&(container->msg), &msg, sizeof(vfsmsg_t));
                    Q_END(&(nodes[msg.rw.ino]->msgs), container);
                    msg.cmd = VFS_HOLD;
                }
            } else {        /* Read or Write requests in the pipe */
                container = (vfsmsg_container_t*) Q_FIRST(nodes[msg.rw.ino]->msgs);
                if (container->msg.cmd == msg.cmd) {
                    /* Same request type, save it */
                    container = (vfsmsg_container_t*) kmalloc(sizeof(vfsmsg_container_t));
                    memcpy(&(container->msg), &msg, sizeof(vfsmsg_t));
                    Q_END(&(nodes[msg.rw.ino]->msgs), container);
                    msg.cmd = VFS_HOLD;
                } else {
                    switch (container->msg.cmd) {
                      case VFS_READC:
                        container->msg.rw.data = msg.rw.data;
                      break;
                      case VFS_WRITEC:
                        msg.rw.data = container->msg.rw.data;
                      break;
                    }
                    /* release waiting task */
                    container->msg.cmd = VFS_REPEAT;
                    container->msg.rw.bnum = 0;
                    sendrec(client, &(container->msg), sizeof(vfsmsg_t));
                    kfree(Q_REMV(&(nodes[msg.rw.ino]->msgs), container));
                }
            }
            msg.rw.bnum = 0;
            break;
        }
        send(client, &msg);
    }
}


