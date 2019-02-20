#include "../kernel/kernel.h"

#include "../servers/vfs.h"
#include "../lib/queue.h"
#include "../lib/mstddef.h"

#include "drv.h"

#include <string.h>



#define MF_MAX_NODES 8
#define MF_MAX_ENTRIES 16

#define MF_DIR      0x01


typedef struct direntry_s {
    char    name[6];
    int     ino;
} direntry_t;


typedef struct mfnode_s {
    char    refcnt;
    char    links;
    char    flags;
    union {
        char        file[MF_MAX_ENTRIES * sizeof(direntry_t)];
        direntry_t  entry[MF_MAX_ENTRIES];
    };
    int     size;
} mfnode_t;


int mf_create_node (mfnode_t** nodes) {
    int ino;
    for (ino = 0; ino != MF_MAX_NODES; ino++) {
        if (nodes[ino]) {
            continue;
        }
    }
    if (ino == MF_MAX_NODES) {
        return -1;
    }
    nodes[ino] = (mfnode_t*) kmalloc(sizeof(mfnode_t));
    nodes[ino]->size = 0;
    nodes[ino]->refcnt = 0;
    nodes[ino]->links = 0;
    return ino;
}

int mf_link (mfnode_t* dirnode, char* name, int ino) {
    int i;
    for (i = 0; i != MF_MAX_ENTRIES; i++) {
        if (dirnode->entry[i].ino == -1) {
            dirnode->entry[i].ino = ino;
            strncpy(dirnode->entry[i].name, name, 6);
            return i;
        }
    }
    return -1;
}

void memfile (void* args UNUSED) {
    pid_t client;
    vfsmsg_t msg;
    int i;
    mfnode_t** nodes = (mfnode_t**)kmalloc(sizeof(mfnode_t*) * MF_MAX_NODES);
    memset(nodes, 0, (sizeof(mfnode_t*) * MF_MAX_NODES));

    kirqdis();

    mf_create_node(nodes);  /* root dir */
    nodes[msg.link.ino]->flags |= MF_DIR;
    for (i = 0; i != MF_MAX_ENTRIES; i++) {
        nodes[msg.link.ino]->entry[i].ino = -1;
    }

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case VFS_MKNOD:
            msg.mknod.ino = mf_create_node(nodes);
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
            if (nodes[msg.iget.ino]->refcnt ||
                nodes[msg.iget.ino]->links) {
                /* more refs */
                break;
            }
            kfree(nodes[msg.iget.ino]);
            nodes[msg.iget.ino] = NULL;
            break;

          case VFS_WRITEC:
            if (msg.rw.pos >= 127) {
                msg.rw.data = EOF;
                msg.rw.bnum = 0;
            } else {
                nodes[msg.rw.ino]->size = msg.rw.pos;
                nodes[msg.rw.ino]->file[msg.rw.pos] = (char) msg.rw.data;
                msg.rw.bnum = 1;
                nodes[msg.rw.ino]->size += msg.rw.bnum;
            }
            break;
          case VFS_READC:
            if ((msg.rw.pos >= 127) ||
                (msg.rw.pos >= nodes[msg.rw.ino]->size)) {
                msg.rw.data = EOF;
                msg.rw.bnum = 0;
            } else {
                msg.rw.data = nodes[msg.rw.ino]->file[msg.rw.pos];
                msg.rw.bnum = 1;
            }
            break;
        }
        send(client, &msg);
    }
}


