#include "../kernel/kernel.h"

#include "../servers/vfs.h"
#include "../lib/queue.h"
#include "../lib/mstddef.h"

#include "drv.h"

#include <string.h>



#define MF_MAX_NODES 8
#define MF_MAX_ENTRIES 16

#define MF_DIR      0x01


typedef struct mf_direntry_s {
    char    name[6];
    int     ino;
} mf_direntry_t;


typedef struct mfnode_s {
    char    refcnt;
    char    links;
    char    flags;
    union {
        char            file[MF_MAX_ENTRIES * sizeof(mf_direntry_t)];
        mf_direntry_t   entry[MF_MAX_ENTRIES];
    };
    union {
        int     size;
        int     entries;
    };
} mfnode_t;


int mf_create_node (mfnode_t** nodes, char flags) {
    int ino;
    int i;
    for (ino = 0; ino != MF_MAX_NODES; ino++) {
        if (!nodes[ino]) {
            nodes[ino] = (mfnode_t*) kmalloc(sizeof(mfnode_t));
            memset (nodes[ino], 0, sizeof(mfnode_t));
            nodes[ino]->flags = flags;
            if (nodes[ino]->flags & MF_DIR) {
                for (i = 0; i != MF_MAX_ENTRIES; i++) {
                    nodes[ino]->entry[i].ino = -1;
                }
            }
            return ino;
        }
    }
    return -1;
}

int mf_link (mfnode_t* dirnode, char* name, int ino) {
    int i;
    for (i = 0; i != MF_MAX_ENTRIES; i++) {
        if (dirnode->entry[i].ino == -1) {
            dirnode->entry[i].ino = ino;
            dirnode->entries++;
            strncpy(dirnode->entry[i].name, name, 6);
            return i;
        }
    }
    return -1;
}

int mf_get_direntry (mfnode_t* dirnode, char* name) {
    int i;
    for (i = 0; i != MF_MAX_ENTRIES; i++) {
        if (dirnode->entry[i].ino == -1) {
            continue;
        }
        if (!strcmp(dirnode->entry[i].name, name)) {
            return i;
        }
    }
    return -1;
}

void memfile (void* args UNUSED) {
    pid_t client;
    vfsmsg_t msg;
    int ino;
    mfnode_t** nodes = (mfnode_t**)kmalloc(sizeof(mfnode_t*) * MF_MAX_NODES);
    memset(nodes, 0, (sizeof(mfnode_t*) * MF_MAX_NODES));

    kirqdis();

    ino = mf_create_node(nodes, MF_DIR);  /* root dir */
    mf_link(nodes[ino], ".", ino);
    nodes[ino]->links += 1;

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case VFS_MKNOD:
            msg.mknod.ino = mf_create_node(nodes, 0);
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

          case VFS_GET_DIRENTRY:
            if (nodes[msg.link.ino]->flags & MF_DIR) {
                msg.link.ino = mf_get_direntry(nodes[msg.link.ino], msg.link.name);
            } else {
                msg.link.ino = -1; /* Not a directory */
            }
            break;
        }
        send(client, &msg);
    }
}


