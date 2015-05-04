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
            msg.param.interrupt.data = UDR0; /* this will clear RXC flag */
            msg.param.interrupt.cmd = DM_READC_ANS;
            break;
          case EVENT_USART0TX:
            UCSR0B &= ~(1<<TXCIE0); /* Disable TXC interrupt */
            /* Executing the interrupt handler clears TXC flag automatically */
            msg.param.interrupt.cmd = DM_WRITEC_ANS;
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
    msgq_t*  elem;

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
                msg.cmd = DM_MKDEV_ANS; 
            }
            break;

          case DM_READC:
            elem = (msgq_t*)(Q_FIRST(rd_q));
            if (elem && elem->msg.cmd == DM_INTERRUPT) {
                msg.param.rwc.data = elem->msg.param.interrupt.data;
                kfree(Q_REMV(&rd_q, elem));
            } else {
                elem = (msgq_t*) kmalloc(sizeof(msgq_t));
                memcpy(&(elem->msg), &msg, sizeof(msg));
                Q_END(&rd_q, elem);
                msg.cmd = DM_DONTREPLY;
            }
            break;
          
          case DM_INTERRUPT:
            switch (msg.param.interrupt.cmd) {
              case DM_READC_ANS:
                if(msg.param.interrupt.data == 0x04){ /* Ctrl + D */
                    msg.param.interrupt.data = EOF;
                }else{ 
                    usart0_wr(msg.param.interrupt.data); /* ECHO */
                }
                elem = (msgq_t*)(Q_FIRST(rd_q));
                if (elem && elem->msg.cmd == DM_READC) {
                    msg.client = elem->msg.client;
                    msg.param.rwc.data = msg.param.interrupt.data;
                    kfree(Q_REMV(&rd_q, elem));
                } else {
                    elem = (msgq_t*) kmalloc(sizeof(msgq_t));
                    memcpy(&(elem->msg), &msg, sizeof(msg));
                    Q_END(&rd_q, elem);
                    msg.cmd = DM_DONTREPLY;
                }
                break;
            }
            break;

          case DM_WRITEC:
            usart0_wr(msg.param.rwc.data); 
            msg.cmd = DM_WRITEC_ANS;
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

void devnull (void) {
    pid_t client;
    dmmsg_t msg;  

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
            case DM_MKDEV:      msg.cmd = DM_MKDEV_ANS;     break;    
            case DM_INTERRUPT:  msg.cmd = DM_DONTREPLY;     break;
            case DM_WRITEC:     msg.cmd = DM_WRITEC_ANS;    break;
            case DM_READC:      
                msg.cmd = DM_READC_ANS;    
                msg.param.rwc.data = EOF; 
                break;
        }
        send(client, &msg);
    }
}

/*
================================================================================
 */

void memfile (void) {
    pid_t client;
    dmmsg_t msg;  
    char*   file;

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case DM_MKDEV:
            file = (char*)kmalloc(128);
            msg.cmd = DM_MKDEV_ANS;
            break;

          case DM_INTERRUPT:
            msg.cmd = DM_DONTREPLY;
            break;

          case DM_WRITEC:
            if (msg.param.rwc.pos > 127) {
                msg.param.rwc.data = EOF;
            } else {
                file[msg.param.rwc.pos] = (char) msg.param.rwc.data;
            }
            msg.cmd = DM_WRITEC_ANS;
            break;
          case DM_READC:
            if (msg.param.rwc.pos > 127) {
                msg.param.rwc.data = EOF;
            } else {     
                msg.param.rwc.data = file[msg.param.rwc.pos];
            }
            msg.cmd = DM_READC_ANS;
            break;
        }
        send(client, &msg);
    }
}

/*
================================================================================
 */

void portdevA (void) {
    pid_t client;
    dmmsg_t msg;

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
            case DM_MKDEV:      msg.cmd = DM_MKDEV_ANS;     break;    
            case DM_INTERRUPT:  msg.cmd = DM_DONTREPLY;     break;
            case DM_WRITEC:
                DDRA = 0xFF;
                PORTA = (int)(msg.param.rwc.data);   
                msg.cmd = DM_WRITEC_ANS;
                break;
            case DM_READC:
                DDRA = 0x00;
                msg.param.rwc.data = (int) PINA;
                msg.cmd = DM_READC_ANS;
                break;
        }    
        send(client, &msg);
    }
}

/*
================================================================================
 */
