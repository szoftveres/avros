#include <avr/io.h>
#include "../kernel/kernel.h"

#include "../servers/vfs.h"
#include "../lib/queue.h"
#include "../lib/mstddef.h"

#include "drv.h"


void usart0_event (void* args UNUSED) {
    pid_t       driver;

    {
        vfsmsg_t    msg;
        driver = receive(TASK_ANY, &msg, sizeof(msg));

        UBRR0H = 0;    /* 9600 BAUD */
        UBRR0L = 103;  /* 9600 BAUD */
        UCSR0C = (1<<USBS0)|(3<<UCSZ00);
        UCSR0B = (1<<RXEN0)|(1<<TXEN0);

        send(driver, &msg);
    }

    kirqdis();

    while(!(UCSR0A & (1<<UDRE0))) yield();
//    if (!(UCSR0A & (1<<TXC0))) {
        /* Generate the first TXC interrupt */
        //UCSR0A |= (1<<TXC0);
        UDR0 = (unsigned char) '\n';
//    }

    while (1) {
        UCSR0B |= (1<<RXCIE0); /* Re-Enable RXC interrupt */
        UCSR0B |= (1<<TXCIE0); /* Re-Enable TXC interrupt */
        switch (waitevent(EVENT_USART0RX | EVENT_USART0TX)) {
          case EVENT_USART0RX:
            UCSR0B &= ~(1<<RXCIE0); /* Disable RXC interrupt */
            vfs_rd_interrupt(driver);
            break;
          case EVENT_USART0TX:
            UCSR0B &= ~(1<<TXCIE0); /* Disable TXC interrupt */
            /* Executing the interrupt handler clears TXC flag automatically */
            vfs_wr_interrupt(driver);
            break;
        }
    }
}

void
usart0_serve_write(q_head_t* wr_q, vfsmsg_t *msg, int cmd_to_be_served) {
    vfsmsg_container_t*   container;

    container = (vfsmsg_container_t*)(Q_FIRST(*wr_q));
    if (container && container->msg.cmd == cmd_to_be_served) {
        /* Request can be served, reply */
        char i = (msg->cmd == VFS_WR_INTERRUPT);
        UDR0 = (unsigned char) (i ? container->msg : *msg).rw.data;
        msg->client = (i ? container->msg : *msg).client;
        kfree(Q_REMV(wr_q, container));
        msg->rw.bnum = 0;
        msg->cmd = VFS_FINAL;
    } else {
        /* Save the request and hold */
        container = (vfsmsg_container_t*) kmalloc(sizeof(vfsmsg_container_t));
        memcpy(&(container->msg), msg, sizeof(vfsmsg_t));
        Q_END(wr_q, container);
        msg->cmd = VFS_HOLD;
    }
}


void
usart0_print_char (q_head_t* wr_q, int c) {
    vfsmsg_t msg;
    msg.cmd = VFS_WRITEC;
    msg.client = NULL;
    msg.rw.data = c;
    usart0_serve_write(wr_q, &msg, VFS_WR_INTERRUPT);
/*
    msg.cmd = VFS_REPEAT;
    sendrec(client, &msg, sizeof(vfsmsg_t));
*/
}

/*
 * **************************
 */

void
usart_serve_read(q_head_t* rd_q, vfsmsg_t *msg, int cmd_to_be_served) {
    vfsmsg_container_t*   container;

    container = (vfsmsg_container_t*)(Q_FIRST(*rd_q));
    if (container && container->msg.cmd == cmd_to_be_served) {
        /* Request can be served, reply */
        char i = (msg->cmd == VFS_RD_INTERRUPT);
        msg->client = (i ? container->msg : *msg).client;
        msg->rw.data = (i ? *msg : container->msg).interrupt.data;
        kfree(Q_REMV(rd_q, container));
        msg->rw.bnum = 0;
        msg->cmd = VFS_FINAL;
    } else {
        /* Save the request and hold */
        container = (vfsmsg_container_t*) kmalloc(sizeof(vfsmsg_container_t));
        memcpy(&(container->msg), msg, sizeof(vfsmsg_t));
        Q_END(rd_q, container);
        msg->cmd = VFS_HOLD;
    }
}


void
usart_reply_char (pid_t client, q_head_t* rd_q, int c) {
    vfsmsg_t msg;

    msg.cmd = VFS_RD_INTERRUPT;
    msg.client = NULL;
    msg.interrupt.data = c;
    usart_serve_read(rd_q, &msg, VFS_READC);
    if (msg.cmd != VFS_HOLD) {
        msg.cmd = VFS_REPEAT;
        sendrec(client, &msg, sizeof(vfsmsg_t));
    }
}


void
tty_flush (pid_t client, q_head_t* rd_q, char* buf, int* idx) {
    int    i;
    for (i = 0; i != *idx; i++) {
        usart_reply_char(client, rd_q, (int) buf[i]);
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

    kirqdis();
    q_init(&rd_q);
    q_init(&wr_q);

    tbuf = kmalloc(128);
    idx = 0;

    /* Setting up interrupt handler */
    client = createtask(TASK_PRIO_RT, PAGE_INVALID);
    allocatestack(client, DEFAULT_STACK_SIZE-64);
    setuptask(client, usart0_event, NULL, NULL);
    starttask(client);
    sendrec(client, &msg, sizeof(msg));


    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));

        switch (msg.cmd) {
          case VFS_READC:
            usart_serve_read(&rd_q, &msg, VFS_RD_INTERRUPT);
            break;
          case VFS_WRITEC:
            usart0_serve_write(&wr_q, &msg, VFS_WR_INTERRUPT);
            break;
          case VFS_RD_INTERRUPT:
            msg.interrupt.data = UDR0;  /* Clear RX interrupt flag */
            if (!ttymode) {
                usart_serve_read(&rd_q, &msg, VFS_READC);
            } else {
                switch (msg.interrupt.data) {
                  case 0x04:        /* Ctrl + D */
                    if (!idx) {
                        usart_reply_char(client, &rd_q, EOF);
                    } else {
                        usart0_print_char(&wr_q, '\n');
                        tty_flush(client, &rd_q, tbuf, &idx);
                    }
                    break;
                  case 0x03:        /* Ctrl + C */
                    idx = 0;
                    usart0_print_char(&wr_q, '\n');
                    break;
                  case 0x08:        /* Backspace */
                    if (idx) {
                        idx--;
//                        usart0_print_char(&wr_q, msg.interrupt.data);
                    }
                    break;
                  case '\r':        /* NewLine */
//                    break;
                  case '\n':        /* NewLine */
//                    usart0_print_char(&wr_q, msg.interrupt.data);
                    tbuf[idx++] = msg.interrupt.data;
                    tty_flush(client, &rd_q, tbuf, &idx);
                    break;
                  default:
//                    usart0_print_char(&wr_q, msg.interrupt.data);
                    tbuf[idx++] = msg.interrupt.data;
                    break;
                }
                msg.cmd = VFS_HOLD;
            }
            break;
          case VFS_WR_INTERRUPT:
            usart0_serve_write(&wr_q, &msg, VFS_WRITEC);
            break;
        }
        send(client, &msg);
    }
}


