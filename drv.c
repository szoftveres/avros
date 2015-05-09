#include "kernel.h"
#include "drv.h"
#include "dm.h"
#include <avr/io.h>
#include "queue.h"

/*
================================================================================
 */


void usart0_wr (int data) {
    while(!(UCSR0A & (1<<UDRE0))) yield();
    UDR0 = (unsigned char) data;
}

void usart0_event (void) {
    dmmsg_t     msg;
    pid_t       driver;
    pid_t       manager;

    driver = receive(TASK_ANY, &msg, sizeof(msg));
    manager = msg.client;
    send(driver, &msg);

    kirqdis();

    while (1) {
        UCSR0B |= (1<<RXCIE0);      /* Enable RXC interrupt */
/*XXX*/ //   UCSR0B |= (1<<TXCIE0); /* Enable TXC interrupt */
        switch (waitevent(EVENT_USART0RX | EVENT_USART0TX)) {
          case EVENT_USART0RX:
            UCSR0B &= ~(1<<RXCIE0); /* Disable RXC interrupt */
            msg.interrupt.data = UDR0; /* this will clear RXC flag */
            msg.interrupt.cmd = DM_READC;
            break;
          case EVENT_USART0TX:
            UCSR0B &= ~(1<<TXCIE0); /* Disable TXC interrupt */
            /* Executing the interrupt handler clears TXC flag automatically */
            msg.interrupt.cmd = DM_WRITEC;
            break;
        }
        msg.cmd = DM_INTERRUPT;
        msg.client = driver;
        send(manager, &msg);
    }
}

/*
 *
 */

void usart0 (void) {    
    pid_t client;
    dmmsg_t msg;  
    q_head_t rd_q;
    q_head_t wr_q;
    dmmsg_t*  elem;

    q_init(&rd_q);
    q_init(&wr_q);

    UBRR0H = 0;    /* 9600 BAUD */
    UBRR0L = 103;  /* 9600 BAUD */
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        
        switch(msg.cmd){

          case DM_MKDEV: {
                pid_t       interrupt;
                interrupt = addtask(TASK_PRIO_RT);
                launchtask(interrupt, usart0_event, DEFAULT_STACK_SIZE);
                msg.client = client;
                sendrec(interrupt, &msg, sizeof(msg));
            }
            break;
          case DM_IGET:
            msg.iget.ans.mode = S_IFCHR;
            break;
          case DM_READC:
            elem = (dmmsg_t*)(Q_FIRST(rd_q));
            if (elem && elem->cmd == DM_INTERRUPT) {
                msg.rw.data = elem->interrupt.data;
                kfree(Q_REMV(&rd_q, elem));
            } else {
                elem = (dmmsg_t*) kmalloc(sizeof(dmmsg_t));
                memcpy(elem, &msg, sizeof(msg));
                Q_END(&rd_q, elem);
                msg.cmd = DM_DONTREPLY;
            }
            break;
          
          case DM_INTERRUPT:
            switch (msg.interrupt.cmd) {
              case DM_READC:
                if(msg.interrupt.data == 0x04){ /* Ctrl + D */
                    msg.interrupt.data = EOF;
                }else{ 
                    usart0_wr(msg.interrupt.data); /* ECHO */
                }
                elem = (dmmsg_t*)(Q_FIRST(rd_q));
                if (elem && elem->cmd == DM_READC) {
                    msg.client = elem->client;
                    msg.rw.data = msg.interrupt.data;
                    kfree(Q_REMV(&rd_q, elem));
                } else {
                    elem = (dmmsg_t*) kmalloc(sizeof(dmmsg_t));
                    memcpy(elem, &msg, sizeof(msg));
                    Q_END(&rd_q, elem);
                    msg.cmd = DM_DONTREPLY;
                }
                break;
            }
            break;

          case DM_WRITEC:
            usart0_wr(msg.rw.data); 
            break; 
        }
        send(client, &msg);
    }
}




void
dputc (int data) {
     usart0_wr(data);
     //data = data;
}

void
dputs (const char* str) {
    while(*str){
        dputc(*str++);
    };
}

void
dputu (unsigned int num) {
    if(num/10){
        dputu(num/10);
    }
    dputc((num%10) + '0');
}


/*
================================================================================
 */


#define MF_MAX_NODES 8

typedef struct mfnode_s {
    char    file[128];
    int     size;
    char    refcnt;
    char    links;
    mode_t  mode;
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

void memfile (void) {
    pid_t client;
    dmmsg_t msg;
    mode_t  mode;
    mfnode_t** nodes = (mfnode_t**)kmalloc(sizeof(mfnode_t*) * MF_MAX_NODES); 

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case DM_MKNOD:
            mode = msg.mknod.mode;
            msg.mknod.ino = mf_find_empty_node(nodes);
            nodes[msg.mknod.ino] = (mfnode_t*) kmalloc(sizeof(mfnode_t));
            nodes[msg.mknod.ino]->size = 0;
            nodes[msg.mknod.ino]->refcnt = 0;
            nodes[msg.mknod.ino]->links = 0;
            nodes[msg.mknod.ino]->mode = mode;
            break;

          case DM_LINK:
            nodes[msg.link.ask.ino]->links += 1;
            break;
          case DM_UNLINK:
            nodes[msg.unlink.ask.ino]->links -= 1;
            if (nodes[msg.unlink.ask.ino]->refcnt || 
                nodes[msg.unlink.ask.ino]->links) {
                /* more refs */
                break;
            }
            kfree(nodes[msg.unlink.ask.ino]);
            nodes[msg.unlink.ask.ino] = NULL;
            break;

          case DM_IGET:
            nodes[msg.iget.ask.ino]->refcnt += 1;
            msg.iget.ans.mode = nodes[msg.iget.ask.ino]->mode;
            break;
          case DM_IPUT:
            nodes[msg.iput.ask.ino]->refcnt -= 1;
            if (nodes[msg.iput.ask.ino]->refcnt || 
                nodes[msg.iput.ask.ino]->links) {
                /* more refs */
                break;
            }
            kfree(nodes[msg.iput.ask.ino]);
            nodes[msg.iput.ask.ino] = NULL;
            break;

          case DM_WRITEC:
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
          case DM_READC:
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
    char    refcnt;
    char    links;
    mode_t  mode;
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

void pipedev (void) {
    pid_t client;
    dmmsg_t msg;
    mode_t mode;
    pdnode_t** nodes = (pdnode_t**)kmalloc(sizeof(pdnode_t*) * PD_MAX_NODES);
    
    memset(nodes, 0, (sizeof(pdnode_t*) * PD_MAX_NODES));

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch (msg.cmd) {
          case DM_MKNOD:
            mode = msg.mknod.mode;
            msg.mknod.ino = pd_find_empty_node(nodes);
            nodes[msg.mknod.ino] = (pdnode_t*)kmalloc(sizeof(pdnode_t));
            nodes[msg.mknod.ino]->refcnt = 0;
            nodes[msg.mknod.ino]->links = 0;
            nodes[msg.mknod.ino]->mode = mode;
            break;


          case DM_LINK:
            nodes[msg.link.ask.ino]->links += 1;
            break;
          case DM_UNLINK:
            nodes[msg.unlink.ask.ino]->links -= 1;
            if (nodes[msg.unlink.ask.ino]->refcnt || 
                nodes[msg.unlink.ask.ino]->links) {
                /* more refs */
                break;
            }
            kfree(nodes[msg.unlink.ask.ino]);
            nodes[msg.unlink.ask.ino] = NULL;
            break;

          case DM_IGET:
            nodes[msg.iget.ask.ino]->refcnt += 1;
            msg.iget.ans.mode = nodes[msg.iget.ask.ino]->mode;
            break;
          case DM_IPUT:
            nodes[msg.iput.ask.ino]->refcnt -= 1;
            if (nodes[msg.iput.ask.ino]->refcnt || 
                nodes[msg.iput.ask.ino]->links) {
                /* more refs */
                break;
            }
            kfree(nodes[msg.iput.ask.ino]);
            nodes[msg.iput.ask.ino] = NULL;
            break;
          case DM_WRITEC:
            msg.rw.data = EOF;
            break;
          case DM_READC:
            msg.rw.data = EOF;
            break;
        }
        send(client, &msg);
    }
}

/*
================================================================================
 */

