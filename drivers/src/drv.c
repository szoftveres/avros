#include <avr/io.h>
#include "../../kernel/include/kernel.h"

#include "../../servers/include/vfs.h"
#include "../../lib/include/queue.h"
#include "../../lib/include/mstddef.h"

#include "../include/drv.h"
/*
================================================================================
 */


void usart0_event (void* args UNUSED) {
    vfsmsg_t    msg;
    pid_t       manager;
    pid_t       driver;

    driver = receive(TASK_ANY, &msg, sizeof(msg));
    manager = msg.client;

    UBRR0H = 0;    /* 9600 BAUD */
    UBRR0L = 103;  /* 9600 BAUD */
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);

    send(driver, &msg);

    kirqdis();

    while(!(UCSR0A & (1<<UDRE0))) yield();
//    if (!(UCSR0A & (1<<TXC0))) {
        /* Generate the first TXC interrupt */
        //UCSR0A |= (1<<TXC0);
        UDR0 = (unsigned char) '\n';
//    }

    msg.client = driver;
    while (1) {
        UCSR0B |= (1<<RXCIE0); /* Re-Enable RXC interrupt */
        UCSR0B |= (1<<TXCIE0); /* Re-Enable TXC interrupt */
        switch (waitevent(EVENT_USART0RX | EVENT_USART0TX)) {
          case EVENT_USART0RX:
            UCSR0B &= ~(1<<RXCIE0); /* Disable RXC interrupt */
            msg.interrupt.data = UDR0; /* this will clear RXC flag */
            msg.cmd = VFS_RX_INTERRUPT;
            break;
          case EVENT_USART0TX:
            UCSR0B &= ~(1<<TXCIE0); /* Disable TXC interrupt */
            /* Executing the interrupt handler clears TXC flag automatically */
            msg.cmd = VFS_TX_INTERRUPT;
            break;
        }
        msg.client = driver;
        send(manager, &msg);
    }
}

/*
 *
 */

void
usart0_serve_write(q_head_t* wr_q, vfsmsg_t *msg, int cmd) {
    vfsmsg_t*   elem;

    elem = (vfsmsg_t*)(Q_FIRST(*wr_q));
    if (elem && elem->cmd == cmd) {
        char i = (msg->cmd == VFS_TX_INTERRUPT);
        UDR0 = (unsigned char) (i ? elem : msg)->rw.data;
        msg->client = (i ? elem : msg)->client;
        kfree(Q_REMV(wr_q, elem));
        msg->rw.bnum = 0;
        msg->cmd = VFS_FINAL;
    } else {
        elem = (vfsmsg_t*) kmalloc(sizeof(vfsmsg_t));
        memcpy(elem, msg, sizeof(vfsmsg_t));
        Q_END(wr_q, elem);
        msg->cmd = VFS_HOLD;
    }
}


void
usart_serve_read(q_head_t* rd_q, vfsmsg_t *msg, int cmd) {
    vfsmsg_t*   elem;

    elem = (vfsmsg_t*)(Q_FIRST(*rd_q));
    if (elem && elem->cmd == cmd) {
        char i = (msg->cmd == VFS_RX_INTERRUPT);
        msg->client = (i ? elem : msg)->client;
        msg->rw.data = (i ? msg : elem)->interrupt.data;
        kfree(Q_REMV(rd_q, elem));
        msg->rw.bnum = 0;
        msg->cmd = VFS_FINAL;
    } else {
        elem = (vfsmsg_t*) kmalloc(sizeof(vfsmsg_t));
        memcpy(elem, msg, sizeof(vfsmsg_t));
        Q_END(rd_q, elem);
        msg->cmd = VFS_HOLD;
    }
}


void
usart_reply_char (q_head_t* rd_q, int c) {
    vfsmsg_t msg;
    msg.cmd = VFS_RX_INTERRUPT;
    msg.client = NULL;
    msg.rw.data = c;
    usart_serve_read(rd_q, &msg, VFS_READC);
}


void
usart0_print_char (q_head_t* wr_q, int c) {
    vfsmsg_t msg;
    msg.cmd = VFS_WRITEC;
    msg.client = NULL;
    msg.rw.data = c;
    usart0_serve_write(wr_q, &msg, VFS_TX_INTERRUPT);
}


void
usart_flush (q_head_t* rd_q, char* buf, int* idx) {
    int    i;
    for (i = 0; i != *idx; i++) {
        usart_reply_char(rd_q, (int) buf[i]);
    }
    *idx = 0;
}



void tty_usart0 (void* args UNUSED) {
    pid_t       client;
    vfsmsg_t    msg;
    q_head_t    rd_q;
    q_head_t    wr_q;
    char        *tbuf;
    int         idx;
    char        ttymode = 1;

    q_init(&rd_q);
    q_init(&wr_q);

    tbuf = kmalloc(128);
    idx = 0;

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));

        switch (msg.cmd) {

          case VFS_MKDEV: {
                pid_t       task;
                task = cratetask(TASK_PRIO_RT, PAGE_INVALID);
                allocatestack(task, DEFAULT_STACK_SIZE-64);
                setuptask(task, usart0_event, NULL, NULL);
                starttask(task);
                msg.client = client;
                sendrec(task, &msg, sizeof(msg));
            }
            break;

          case VFS_IGET:
            break;
          case VFS_IPUT:
            break;
          case VFS_LINK:
            break;
          case VFS_UNLINK:
            break;
          case VFS_READC:
            usart_serve_read(&rd_q, &msg, VFS_RX_INTERRUPT);
            break;
          case VFS_WRITEC:
            usart0_serve_write(&wr_q, &msg, VFS_TX_INTERRUPT);
            break;
          case VFS_RX_INTERRUPT:
            if (!ttymode) {
                usart_serve_read(&rd_q, &msg, VFS_READC);
            } else {
                /* Echo */
                usart0_print_char(&wr_q, msg.interrupt.data);
                switch (msg.interrupt.data) {
                  case 0x04:        /* Ctrl + D */
                    if (!idx) {
                        usart_reply_char(&rd_q, EOF);
                    } else {
                        usart_flush(&rd_q, tbuf, &idx);
                    }
                    break;
                  case 0x03:        /* Ctrl + C */
                    idx = 0;
                    tbuf[idx++] = '\n';
                    usart_flush(&rd_q, tbuf, &idx);
                    break;
                  case 0x08:        /* Backspace */ 
                    if (idx) {
                        idx--;
                    }
                    break;
                  case '\r':        /* NewLine */
                    break;
                  case '\n':        /* NewLine */
                    tbuf[idx++] = msg.interrupt.data;
                    usart_flush(&rd_q, tbuf, &idx);
                    break;
                  default:
                    tbuf[idx++] = msg.interrupt.data;
                    break;                    
                }
                msg.cmd = VFS_HOLD;
            }
            break;
          case VFS_TX_INTERRUPT:
            usart0_serve_write(&wr_q, &msg, VFS_WRITEC);
            break;
        }
        send(client, &msg);
    }
}





/*
================================================================================
 */


#define MF_MAX_NODES 8

typedef struct mfnode_s {
    char    refcnt;
    char    links;
    char    file[128];
    int     size;
} mfnode_t;

int
mf_find_empty_node (mfnode_t** list) {
    int i;
    for (i = 0; i != MF_MAX_NODES; i++) {
        if (!list[i]) {
            return i;
        }
    }
    return (-1);
}

void memfile (void* args UNUSED) {
    pid_t client;
    vfsmsg_t msg;
    mfnode_t** nodes = (mfnode_t**)kmalloc(sizeof(mfnode_t*) * MF_MAX_NODES); 
    memset(nodes, 0, (sizeof(mfnode_t*) * MF_MAX_NODES));

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case VFS_MKNOD:
            msg.mknod.ino = mf_find_empty_node(nodes);
            nodes[msg.mknod.ino] = (mfnode_t*) kmalloc(sizeof(mfnode_t));
            nodes[msg.mknod.ino]->size = 0;
            nodes[msg.mknod.ino]->refcnt = 0;
            nodes[msg.mknod.ino]->links = 0;
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

          case VFS_IGET:
            if (!nodes[msg.iget.ino]) {
                msg.iget.ino = -1;
                break;
            }
            nodes[msg.iget.ino]->refcnt += 1;
            break;
          case VFS_IPUT:
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


/*
================================================================================
 */

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
    pid_t client;
    vfsmsg_t msg;
    vfsmsg_t* msg_p; /* Temporary pointer */

    pdnode_t** nodes = (pdnode_t**)kmalloc(sizeof(pdnode_t*) * PD_MAX_NODES);
    memset(nodes, 0, (sizeof(pdnode_t*) * PD_MAX_NODES));

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

          case VFS_IGET:
            if (!nodes[msg.iget.ino]) {
                msg.iget.ino = -1;
                break;
            }
            nodes[msg.iget.ino]->refcnt += 1;
            break;
          case VFS_IPUT:
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
                    msg_p = (vfsmsg_t*) Q_FIRST(nodes[msg.iget.ino]->msgs);
                    msg_p->rw.data = EOF;
                    msg_p->rw.bnum = 0;
                    msg_p->cmd = VFS_REPEAT;
                    sendrec(client, msg_p, sizeof(vfsmsg_t));
                    kfree(Q_REMV(&(nodes[msg.iget.ino]->msgs), msg_p));
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
                    msg_p = (vfsmsg_t*) kmalloc(sizeof(vfsmsg_t));
                    memcpy(msg_p, &msg, sizeof(vfsmsg_t));
                    Q_END(&(nodes[msg.rw.ino]->msgs), msg_p);
                    msg.cmd = VFS_HOLD;
                }
            } else {        /* Read or Write requests in the pipe */
                msg_p = (vfsmsg_t*) Q_FIRST(nodes[msg.rw.ino]->msgs);
                if (msg_p->cmd == msg.cmd) {
                    /* Same request type, save it */
                    msg_p = (vfsmsg_t*) kmalloc(sizeof(vfsmsg_t));
                    memcpy(msg_p, &msg, sizeof(vfsmsg_t));
                    Q_END(&(nodes[msg.rw.ino]->msgs), msg_p);
                    msg.cmd = VFS_HOLD;
                } else {
                    switch (msg_p->cmd) {
                      case VFS_READC:
                        msg_p->rw.data = msg.rw.data;
                      break;
                      case VFS_WRITEC:
                        msg.rw.data = msg_p->rw.data;
                      break;
                    }
                    /* release waiting task */
                    msg_p->cmd = VFS_REPEAT;
                    msg_p->rw.bnum = 0;
                    sendrec(client, msg_p, sizeof(vfsmsg_t));
                    kfree(Q_REMV(&(nodes[msg.rw.ino]->msgs), msg_p));
                }
            }
            msg.rw.bnum = 0;
            break;
        }
        send(client, &msg);
    }
}

/*
================================================================================
 */

